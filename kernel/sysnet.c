//
// network system calls.
//

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

struct sock {
  struct sock *next; // the next socket in the list
  uint32 raddr;      // the remote IPv4 address
  uint16 lport;      // the local UDP port number
  uint16 rport;      // the remote UDP port number
  struct spinlock lock; // protects the rxq
  struct mbufq rxq;  // a queue of packets waiting to be received
};

static struct spinlock lock;
static struct sock *sockets;

void
sockinit(void)
{
  initlock(&lock, "socktbl");
}

int
sockalloc(struct file **f, uint32 raddr, uint16 lport, uint16 rport)
{
  struct sock *si, *pos;

  si = 0;
  *f = 0;
  if ((*f = filealloc()) == 0)
    goto bad;
  if ((si = (struct sock*)kalloc()) == 0)
    goto bad;

  // initialize objects
  si->raddr = raddr;
  si->lport = lport;
  si->rport = rport;
  initlock(&si->lock, "sock");
  mbufq_init(&si->rxq);
  (*f)->type = FD_SOCK;
  (*f)->readable = 1;
  (*f)->writable = 1;
  (*f)->sock = si;

  // add to list of sockets
  acquire(&lock);
  pos = sockets;
  while (pos) {
    if (pos->raddr == raddr &&
        pos->lport == lport &&
	pos->rport == rport) {
      release(&lock);
      goto bad;
    }
    pos = pos->next;
  }
  si->next = sockets;
  sockets = si;
  release(&lock);
  return 0;

bad:
  if (si)
    kfree((char*)si);
  if (*f)
    fileclose(*f);
  return -1;
}

//
// Your code here.
//
// Add and wire in methods to handle closing, reading,
// and writing for network sockets.
//

void sockclose(struct sock *s){
  struct sock *prev;

  acquire(&lock);
  acquire(&s->lock);

  if(sockets == s)
    sockets = s->next;
  else{
    for(prev = sockets; prev; prev = prev->next){
      if(prev->next == s){
        prev->next = prev->next->next;
        break;
      }
    }
  }

  while(!mbufq_empty(&s->rxq))
    mbuffree(mbufq_pophead(&s->rxq));

  release(&s->lock);
  kfree(s);
  release(&lock);
}

int sockwrite(struct sock *s, uint64 addr, int n){
  struct mbuf *m;
  struct proc *pr = myproc();

  if(n > MBUF_SIZE - MBUF_DEFAULT_HEADROOM)
    return -1;

  if((m = mbufalloc(MBUF_DEFAULT_HEADROOM)) == 0)
    return -1;
  copyin(pr->pagetable, m->head, addr, n);
  mbufput(m, n);
  acquire(&s->lock);
  net_tx_udp(m, s->raddr, s->lport, s->rport);
  release(&s->lock);
  return n;
}

int sockread(struct sock *s, uint64 addr, int n){
  struct mbuf *m;
  struct proc *pr = myproc();
  char *data;
  int received_bytes = 0;

  acquire(&s->lock);
  while(mbufq_empty(&s->rxq)){
    if(myproc()->killed){
      release(&s->lock);
      return -1;
    }
    sleep(s, &s->lock);
  }
  if(!mbufq_empty(&s->rxq) && n > 0){
    if(n >= s->rxq.head->len){
      m = mbufq_pophead(&s->rxq);
      data = m->head;
      copyout(pr->pagetable, addr, data, m->len);
      n -= m->len;
      addr += m->len;
      received_bytes += m->len;
      mbuffree(m);
    } else {
      m = s->rxq.head;
      data = mbufpull(m, n);
      copyout(pr->pagetable, addr, data, n);
      received_bytes += n;
      n = 0;
    }
  }
  release(&s->lock);
  return received_bytes;
}

// called by protocol handler layer to deliver UDP packets
void
sockrecvudp(struct mbuf *m, uint32 raddr, uint16 lport, uint16 rport)
{
  struct sock *s;


  acquire(&lock);
  for(s = sockets; s; s = s->next){
    if(s->raddr == raddr && s->lport == lport && s->rport == rport)
      break;
  }
  release(&lock);

  if(!s){
    return mbuffree(m);
  }

  acquire(&s->lock);
  mbufq_pushtail(&s->rxq, m);
  release(&s->lock);

  wakeup(s);
}
