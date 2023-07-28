// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

static void *ktake(int hart);
static void  kput(void *pa, int hart);
static void *kborrow(int hart);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock[NCPU + 1];
  struct run *freelist[NCPU + 1];
} kmem;

static char locknames[NCPU + 1][8] = {
  "kmem.0", "kmem.1", "kmem.2", "kmem.3", "kmem.4", "kmem.5", "kmem.6","kmem.7", "kmem.8"
};

void
kinit()
{
  uint64 pa_start, hart_pages;
  char *p;

  initlock(&kmem.lock[0], locknames[0]);

  pa_start = PGROUNDUP((uint64) end);
  hart_pages = (PHYSTOP - pa_start) / (NCPU * PGSIZE);
  p = (char *) pa_start;
  for (int hart = 1; hart <= NCPU; hart++) {
    for (int i = 0; i < hart_pages; i++) {
      kput(p, hart);
      p += PGSIZE;
    }
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  int hart;

  push_off();
  hart = cpuid();
  pop_off();

  kput(pa, hart);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  int hart;
  void *p;

  push_off();
  hart = cpuid();
  pop_off();

  p = ktake(hart);
  if (!p)
    p = kborrow(hart);

  if(p)
    memset((char*)p, 5, PGSIZE); // fill with junk

  return p;
}

static void *
ktake(int hart)
{
  struct run *r;

  acquire(&kmem.lock[hart]);
  r = kmem.freelist[hart];
  if(r)
    kmem.freelist[hart] = r->next;
  release(&kmem.lock[hart]);

  return (void*)r;
}

static void
kput(void *pa, int hart)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock[hart]);
  r->next = kmem.freelist[hart];
  kmem.freelist[hart] = r;
  release(&kmem.lock[hart]);
}

static void *
kborrow(int hart)
{
  void * p;
  for (int i = 0; i <= NCPU; i++) {
    if (i != hart && (p = ktake(i))) {
      return p;
    }
  }

  return 0;
}
