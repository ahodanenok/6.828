#include "types.h"
#include "param.h"
#include "riscv.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fcntl.h"
#include "fs.h"
#include "file.h"
#include "proc.h"
#include "defs.h"

uint64 mmapcreate(int len, int prot, int flags, struct file *f)
{
  if (!f->writable && (prot & PROT_WRITE) && !(flags & MAP_PRIVATE)) {
    printf("file not writable, but PROT_WRITE\n");
    return -1;
  }

  struct proc *p = myproc();
  uint64 addr;
  struct mm *mm;

  int i = 0;
  for (; i < MMAP_COUNT; i++) {
    if (!p->maps[i].f) {
      break;
    }
  }

  if (i == MMAP_COUNT) {
    return -1;
  }

  addr = MMAP_BLOCK(i);

  mm = &p->maps[i];
  mm->f = f;
  mm->addr = addr;
  mm->len = len;
  mm->prot = prot;
  mm->flags = flags;

  filedup(mm->f);

  return addr;
}

int mmapclose(uint64 addr, int len)
{
  struct proc *p = myproc();
  struct mm *mm;
  pte_t *pte;

  int i = 0;
  for (; i < MMAP_COUNT; i++) {
    if (addr >= p->maps[i].addr && addr < p->maps[i].addr + p->maps[i].len) {
      break;
    }
  }

  if (i == MMAP_COUNT) {
    return 0;
  }

  mm = &p->maps[i];
  if (mm->flags & MAP_SHARED) {
    for (uint64 a = addr; a < addr + len; a += PGSIZE) {
      pte = walk(p->pagetable, a, 0);
      if (pte != 0 && PTE_FLAGS(*pte) & PTE_D) {
        begin_op();
        ilock(mm->f->ip);
        writei(mm->f->ip, 1, a, a - mm->addr, PGSIZE);
        iunlock(mm->f->ip);
        end_op();
      }
    }
  }

  for (uint64 a = addr; a < addr + len; a += PGSIZE) {
    if (walkaddr(p->pagetable, a) > 0) {
      uvmunmap(p->pagetable, a, 1, 1);
    }
  }

  if (mm->addr == addr) {
    mm->addr += len;
  }
  mm->len -= len;
  if (mm->len > 0) {
    return 0;
  }

  fileclose(mm->f);

  mm->f = 0;
  mm->addr = 0;
  mm->len = 0;
  mm->flags = 0;
  mm->prot = 0;

  return 0;
}

int mmaptrap(uint64 addr)
{
  struct proc *p = myproc();
  struct mm *mm;

  mm = 0;
  addr = PGROUNDDOWN(addr);
  for (int i = 0; i < MMAP_COUNT; i++) {
    if (p->maps[i].f && addr >= p->maps[i].addr && addr < p->maps[i].addr + p->maps[i].len) {
      mm = &p->maps[i];
    }
  }

  if (!mm) {
    return -1;
  }

  void *m = kalloc();
  if (!m) {
    panic("failed to allocate page for mmap");
  }
  memset(m, 0, PGSIZE);

  int perms = PTE_U;
  if (mm->prot & PROT_READ) {
    perms |= PTE_R;
  }
  if (mm->prot & PROT_WRITE) {
    perms |= PTE_W;
  }

  if (mappages(p->pagetable, PGROUNDDOWN(addr), PGSIZE, (uint64) m, perms) < 0) {
    panic("failed to map pages for mmap");
  }

  ilock(mm->f->ip);
  readi(mm->f->ip, 1, addr, addr - mm->addr, PGSIZE);
  iunlock(mm->f->ip);

  return 0;
}
