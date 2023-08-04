// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define BUCKET(dev, blockno) ((31 * blockno + dev) % NBUFBUCKET)

struct bcachebucket {
  struct spinlock lock;
  struct buf *head;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct bcachebucket bucket[NBUFBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
  }

  for (int i = 0; i < NBUFBUCKET; i++) {
    initlock(&bcache.bucket[i].lock, "bcache.bucket");
    bcache.bucket[i].head = 0;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b, *c;
  uint bk, ck;

  bk = BUCKET(dev, blockno);
  acquire(&bcache.bucket[bk].lock);
  for(b = bcache.bucket[bk].head; b != 0; b = b->bnext){   
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket[bk].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket[bk].lock);

  while (1) {
    c = 0;

    acquire(&bcache.lock);
    for (int i = 0; i < NBUF; i++) {
      if (bcache.buf[i].refcnt == 0) {
        c = &bcache.buf[i];
        break;
      }
    }
    release(&bcache.lock);

    if (!c) {
      panic("no candidate");
    }

    ck = BUCKET(c->dev, c->blockno);
    acquire(&bcache.bucket[ck].lock);
    if (c->refcnt > 0) {
      release(&bcache.bucket[ck].lock);
      continue;
    }

    if (bcache.bucket[ck].head == c) {
      bcache.bucket[ck].head = c->bnext;
    } else {
      struct buf *w;
      for (w = bcache.bucket[ck].head; w != 0; w = w->bnext) {
        if (w->bnext == c) {
          w->bnext = c->bnext;
          break;
        }
      }
    }

    c->bnext = 0;
    c->dev = dev;
    c->blockno = blockno;
    c->valid = 0;
    c->refcnt = 1;
    release(&bcache.bucket[ck].lock);
    break;
  }

  acquire(&bcache.bucket[bk].lock);
  for(b = bcache.bucket[bk].head; b != 0; b = b->bnext){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      c->refcnt = 0;
      release(&bcache.bucket[bk].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  b = c;
  if (!bcache.bucket[bk].head) {
    bcache.bucket[bk].head = b;
    b->bnext = 0;
  } else {
    b->bnext = bcache.bucket[bk].head;
    bcache.bucket[bk].head = b;
  }
  release(&bcache.bucket[bk].lock);
  acquiresleep(&b->lock);
  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint bk = BUCKET(b->dev, b->blockno);
  acquire(&bcache.bucket[bk].lock);
  b->refcnt--;
  release(&bcache.bucket[bk].lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


