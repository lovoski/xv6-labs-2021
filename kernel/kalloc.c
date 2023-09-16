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

// Ref counter
#define PA2PGREFID(pa) (((pa)-KERNBASE)/PGSIZE)
#define PGREF_MAX_ENTRIES PA2PGREFID(PHYSTOP)
int pageref[PGREF_MAX_ENTRIES];
struct spinlock pgreflock;
#define PA2PGREF(pa) pageref[PA2PGREFID((uint64)pa)]

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
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
// Ref counter - 1, if the counter is 0, release phsical memory
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&pgreflock);
  // check for reference of a physical page
  if (--PA2PGREF(pa) <= 0) {
    // release the physical memory
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);
    r = (struct run*)pa;
    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  release(&pgreflock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
// Ref counter set to 1
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    // initialize page ref to 1
    PA2PGREF(r) = 1;
  }
  return (void*)r;
}

// create new ref for a page
// ref counter + 1
void
krefpage(void *pa)
{
  acquire(&pgreflock);
  // increase page ref by 1
  PA2PGREF(pa)++;
  release(&pgreflock);
}

// copy and remove ref from a page
// ref counter - 1
// if the number of page ref is alreay 1
// do not alloc new memory, return the memory itself
void *
kcopyrefpage(void *pa)
{
  acquire(&pgreflock);
  if (PA2PGREF(pa) <= 1) {
    release(&pgreflock);
    return pa;
  }
  // alloc new phsysical memory
  uint64 npa = (uint64)kalloc();
  if (npa == 0) {
    // alloc failed
    // not enough memory
    release(&pgreflock);
    return 0;
  }
  // copy old data to newly alloc memory
  memmove((void *)npa, (void *)pa, PGSIZE);
  // decrease old page ref by 1
  PA2PGREF(pa)--;
  release(&pgreflock);
  return (void *)npa;
}
