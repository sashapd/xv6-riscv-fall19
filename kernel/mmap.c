#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"
#include "proc.h"
#include "defs.h"

struct vma *
vma_alloc(struct vma *vma, int length)
{
  struct vma *v, *res_vma = 0;
  uint64 end_addr = VMA_START;

  for(v = vma; v != &vma[NVMA]; v++){
    if(v->used == 0)
      res_vma = v;
    else if((uint64)v->addr + v->length >= end_addr)
      end_addr = PGROUNDUP((uint64)v->addr + v->length);
  }

  if(!res_vma || end_addr + length >= TRAPFRAME)
    panic("vma_alloc");

  res_vma->used = 1;
  res_vma->length = length;
  res_vma->addr = end_addr;

  return res_vma;
}

void
vma_free(struct vma *v)
{
  if(v->file)
    fileclose(v->file);
  v->file = 0;
  v->used = 0;
  v->length = 0;
  v->addr = 0;
  v->prot = 0;
  v->flags = 0;
  v->offset = 0;
}

int
vma_clear_pages(pagetable_t pagetable, struct file *f, uint64 va, int size, int offset, int do_write)
{
  uint64 a, last;
  uint64 pa;

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for(;;){
    if(walkaddr(pagetable, a) != 0){
      if(do_write){
        begin_op(ROOTDEV);
        ilock(f->ip);
        if(writei(f->ip, 1, a, offset, PGSIZE) == -1){
          panic("vma_clear_pages");
          end_op(ROOTDEV);
          return -1;
        }
        iunlock(f->ip);
        end_op(ROOTDEV);
      }
      uvmunmap(pagetable, a, PGSIZE, 1);
    }
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
    offset += PGSIZE;
  }
  return 0;
}

int
vma_unmap(struct proc *p, uint64 addr, int length)
{
  struct vma *v = 0, *vma = p->vma;

  for(v = vma; v != &vma[NVMA]; v++){
    if(v->used == 1 && v->addr <= addr && ((v->addr + v->length) > addr) &&
      (v->addr <= (addr + length)) && ((v->addr + v->length) >= addr + length)){
      break;
    }
  }
  if(v == &vma[NVMA])
    return -1;

  if(vma_clear_pages(p->pagetable, v->file, addr, length, v->offset, v->prot == MAP_SHARED) == -1)
    return -1;
  
  if(addr == v->addr && length == v->length)
    vma_free(v);
  else if(addr == v->addr){
    v->offset += v->length - length;
    v->length -= length;
    v->addr = addr + length;
  } else if(addr + length == v->addr + length){
    v->length -= addr - v->addr;
    v->addr = addr;
  } else{
    panic("vma_unmap");
  }

  return 0;
}

int
mmap_alloc(struct vma *vma, uint64 addr, pagetable_t pagetable, int cause)
{
  struct vma *v;
  uint64 pg_addr = PGROUNDDOWN(addr);
  char *mem;
  uint64 flags = 0, offset;

  for(v = vma; v != &vma[NVMA]; v++){
    if(v->used && (uint64)v->addr <= addr && ((uint64)v->addr + v->length) > addr){
      if((cause == 13 && !(v->flags & PTE_R)) || (cause == 15 && !(v->flags & PTE_W)))
        return -1;

      flags = PTE_U | (v->flags & PTE_R) | (v->flags & PTE_W);
      mem = kalloc();
      if(mem == 0){
        return -1;
      }
      memset((char*)mem, 0, PGSIZE);
      if(mappages(pagetable, pg_addr, PGSIZE, (uint64)mem, flags) != 0){
        kfree(mem);
        return -1;
      }
      offset = addr - (uint64)v->addr;
      ilock(v->file->ip);
      if(readi(v->file->ip, 0, (uint64)mem, v->offset + offset, PGSIZE) == -1){
        kfree(mem);
        return -1;
      }
      iunlock(v->file->ip);
      return 0;
    }
  }

  return -1;
}

int
vma_vmcopy(pagetable_t old, pagetable_t new, struct vma *v_old, struct vma *v_new)
{
  uint64 a, last;
  uint64 pa;
  char *mem;
  uint64 flags = PTE_U | (v_new->flags & PTE_R) | (v_new->flags & PTE_W);

  a = PGROUNDDOWN(v_old->addr);
  last = PGROUNDDOWN(v_old->addr + v_old->length - 1);
  for(;;){
    if((pa = walkaddr(old, a)) != 0){
      mem = kalloc();
      if(mem == 0)
        goto err;
      memmove(mem, (void *)pa, PGSIZE);
      if(mappages(new, a, PGSIZE, (uint64)mem, flags) != 0)
        goto err;
    }
    if(a == last)
      break;
    a += PGSIZE;
  }
  return 0;

err:
  panic("vma_vmcopy");
  return -1;
}

void
vma_dup(struct proc *p, struct proc *np)
{
  int i;

  memmove(np->vma, p->vma, sizeof(struct vma) * NVMA);
  for(i = 0; i < NVMA; i++){
    if(np->vma[i].used){
      np->vma[i].file = filedup(np->vma[i].file);
      vma_vmcopy(p->pagetable, np->pagetable, &p->vma[i], &np->vma[i]);
    }
  }
}