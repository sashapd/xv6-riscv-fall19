// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

struct spinlock steal_lock;

void
kinit()
{
  for(int i = 0; i < NCPU; ++i){
    initlock(&kmem[i].lock, "kmem");
  }
  initlock(&steal_lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int cpu_n;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  
  push_off();
  cpu_n = cpuid();

  acquire(&kmem[cpu_n].lock);
  r->next = kmem[cpu_n].freelist;
  kmem[cpu_n].freelist = r;
  release(&kmem[cpu_n].lock);

  pop_off();
}

void steal(int cpu_from, int cpu_to)
{
  struct run *fast = kmem[cpu_from].freelist;
  struct run *prev;
  kmem[cpu_to].freelist = kmem[cpu_from].freelist;
  while(fast){
    fast = fast->next;
    if(fast)
      fast = fast->next;
    kmem[cpu_from].freelist = kmem[cpu_from].freelist->next;
  }

  if(kmem[cpu_from].freelist){
    prev = kmem[cpu_from].freelist;
    kmem[cpu_from].freelist = kmem[cpu_from].freelist->next;
    prev->next = 0;
  }
}

void try_steal(int cpu_n)
{
  acquire(&steal_lock);
  for(int cpu_from = 0; cpu_from < NCPU; ++cpu_from){
    if(cpu_from != cpu_n){
      acquire(&kmem[cpu_from].lock);
      acquire(&kmem[cpu_n].lock);

      if(kmem[cpu_from].freelist){
        steal(cpu_from, cpu_n);

        release(&kmem[cpu_n].lock);
        release(&kmem[cpu_from].lock);
        break;
      }
      release(&kmem[cpu_n].lock);
      release(&kmem[cpu_from].lock);
    }
  }
  release(&steal_lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int cpu_n;

  push_off();
  cpu_n = cpuid();

  if(!kmem[cpu_n].freelist)
    try_steal(cpu_n);

  acquire(&kmem[cpu_n].lock);
  r = kmem[cpu_n].freelist;
  if(r)
    kmem[cpu_n].freelist = r->next;
  release(&kmem[cpu_n].lock);

  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
