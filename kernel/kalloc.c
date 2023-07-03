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

static int   _krefidx(void *pa);
static void  _krefinc(void *pa);
static uint8 _krefdec(void *pa);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

uint8 *refcnt;
uint64 pa_first;

void
kinit()
{
  initlock(&kmem.lock, "kmem");

  int count = (PHYSTOP - PGROUNDUP((uint64) end)) / PGSIZE;
  refcnt = (uint8*) PGROUNDUP((uint64) end);
  memset(refcnt, 1, count);
  pa_first = PGROUNDUP((uint64) refcnt + count);

  freerange((void*)pa_first, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  r = (struct run*)pa;
  acquire(&kmem.lock);
  if (_krefdec((void*) r) == 0) {
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);    

    r->next = kmem.freelist;
    kmem.freelist = r;
  }
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    _krefinc((void*) r);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

static int
_krefidx(void *pa)
{
  return ((uint64) pa - pa_first) / PGSIZE;
}

static void
_krefinc(void *pa)
{
  refcnt[_krefidx((void*) pa)]++;
}

void
krefinc(void *pa)
{
  acquire(&kmem.lock);
  _krefinc(pa);
  release(&kmem.lock);
}

static uint8
_krefdec(void *pa)
{
  uint64 idx = _krefidx(pa);
  if (refcnt[idx] == 0) {
    printf("krefdec: not referenced page pa=%p\n", pa);
    panic("krefdec");
  }
  refcnt[idx]--;

  return refcnt[idx];
}

uint8
krefcnt(void *pa)
{
  uint8 cnt;

  acquire(&kmem.lock);
  cnt = refcnt[_krefidx(pa)];
  release(&kmem.lock);

  return cnt;
}
