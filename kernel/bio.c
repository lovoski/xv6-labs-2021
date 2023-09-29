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

#define PRIME 13

struct {
  struct spinlock lock;
  struct buf list;
} hash[NBUF];

struct {
  struct buf buf[NBUF];
} bcache;

// struct {
//   struct spinlock lock;
//   struct buf buf[NBUF];

//   // Linked list of all buffers, through prev/next.
//   // Sorted by how recently the buffer was used.
//   // head.next is most recent, head.prev is least.
//   struct buf head;
// } bcache;

void
binit(void)
{
  struct buf *b;

  // initlock(&bcache.lock, "bcache");
  for (int i = 0; i < PRIME; ++i) {
    initlock(&hash[i].lock, "bcache");
    hash[i].list.next = 0;
  }

  // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    // bcache.head.next->prev = b;
    // bcache.head.next = b;
    b->blockno = 0;
    b->next = hash[0].list.next;
    hash[0].list.next = b;
  }
}

static struct buf*
subbget(struct buf *rob, uint dev, uint blockno)
{
  int bucket = blockno % PRIME;
  int victim = rob->blockno % PRIME;

  rob->tickstamp = ticks;
  rob->dev = dev;
  rob->blockno = blockno;
  rob->valid = 0;
  rob->refcnt = 1;

  if (victim != bucket) {
    struct buf *pre = &hash[victim].list;
    while (pre->next != rob) pre = pre->next;
    pre->next = rob->next;
    release(&hash[victim].lock);
    rob->next = hash[bucket].list.next;
    hash[bucket].list.next = rob;
  }

  release(&hash[bucket].lock);
  acquiresleep(&rob->lock);
  return rob;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  // acquire(&bcache.lock);

  // Is the block already cached?
  int bucket = blockno % PRIME;
  acquire(&hash[bucket].lock);
  // for(b = bcache.head.next; b != &bcache.head; b = b->next){
  //   if(b->dev == dev && b->blockno == blockno){
  //     b->refcnt++;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  for (b = hash[bucket].list.next; b != 0; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&hash[bucket].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  int i = bucket;
  do {
    if (i != bucket) acquire(&hash[i].lock);
    struct buf *minibuf = 0;
    uint mini = 0xffffffff;
    for (b = hash[i].list.next; b != 0; b = b->next) {
      if (b->refcnt == 0 && mini > b->tickstamp) {
        mini = b->tickstamp;
        minibuf = b;
      }
    }
    if (minibuf != 0)
      return subbget(minibuf, dev, blockno);
    if (i != bucket) release(&hash[i].lock);
    i = (i+1)%PRIME;
  } while(i != bucket);
  panic("bget: no buffers");
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

  int bucket = b->blockno % PRIME;
  acquire(&hash[bucket].lock);
  b->refcnt--;
  if (b->refcnt == 0)
    b->tickstamp = ticks;
  release(&hash[bucket].lock);
  // acquire(&bcache.lock);
  // b->refcnt--;
  // if (b->refcnt == 0) {
  //   // no one is waiting for it.
  //   b->next->prev = b->prev;
  //   b->prev->next = b->next;
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
  
  // release(&bcache.lock);
}

void
bpin(struct buf *b) {
  int bucket = b->blockno % PRIME;
  acquire(&hash[bucket].lock);
  b->refcnt++;
  release(&hash[bucket].lock);
}

void
bunpin(struct buf *b) {
  int bucket = b->blockno % PRIME;
  acquire(&hash[bucket].lock);
  b->refcnt--;
  release(&hash[bucket].lock);
}


