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

void
kinit()
{
  for(int i = 0; i < NCPU; i++) {
    char lockname[6] = "kmem";
    lockname[4] = (char)i + '0';
    lockname[5] = '\0';
    initlock(&kmem[i].lock,lockname);
  }
  //initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  push_off();
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
  pop_off();
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  push_off();
  int cid = cpuid();

  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem[cid].lock);
  r->next = kmem[cid].freelist;
  kmem[cid].freelist = r;
  release(&kmem[cid].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  push_off();
  int cid = cpuid();

  acquire(&kmem[cid].lock);
  r = kmem[cid].freelist;
  if(r)
    kmem[cid].freelist = r->next;
  release(&kmem[cid].lock);

  if (!r) {
    for (int i = 1; i < NCPU; i++) {
      int ncpuid = (cid + i) % NCPU;
      acquire(&kmem[ncpuid].lock);
      r = kmem[ncpuid].freelist;
      if (r) {
        kmem[ncpuid].freelist = r->next;
        release(&kmem[ncpuid].lock);
        break;
      }
      release(&kmem[ncpuid].lock);
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk

  pop_off();
  return (void*)r;
}
