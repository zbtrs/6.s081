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

struct {
  struct spinlock lock[bucketN];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[bucketN];
  struct spinlock biglock;
} bcache;

void
binit(void)
{
  struct buf *b;
  initlock(&bcache.biglock,"bcache");

  for (int i = 0; i < bucketN; i++) {
    char lockname[7] = "bcache";
    lockname[5] = (char)i + '0';
    lockname[6] = '\0';
    initlock(&bcache.lock[i], lockname);
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }
    // Create linked list of buffers
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
    b->next = bcache.head[0].next;
    b->prev = &bcache.head[0];
    bcache.head[0].next->prev = b;
    bcache.head[0].next = b;
  }

}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int bn = blockno % bucketN;  // which bucket to choose

  acquire(&bcache.lock[bn]);
  //acquire(&bcache.biglock);

  // Is the block already cached?
  for(b = bcache.head[bn].next; b != &bcache.head[bn]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[bn]);
      //release(&bcache.biglock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.

  struct buf *tmp;
  tmp = 0;

  for(b = bcache.head[bn].next; b != &bcache.head[bn]; b = b->next){
    if(b->refcnt == 0 && (tmp == 0 || b->timestamp > tmp->timestamp)) {
      tmp = b;
    }
  }

  if (tmp != 0) {
    tmp->dev = dev;
    tmp->blockno = blockno;
    tmp->valid = 0;
    tmp->refcnt = 1;
    release(&bcache.lock[bn]);
    acquiresleep(&tmp->lock);
    return tmp;
  }
    /*
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[bn]);
      //release(&bcache.biglock);
      acquiresleep(&b->lock);
      return b;
    */
  release(&bcache.lock[bn]);
  acquire(&bcache.biglock);
  // payattention: release!
  for (int i = 1; i < bucketN; i++) {
    int anobucket = (bn + i) % bucketN;
    acquire(&bcache.lock[anobucket]);
    for (b = bcache.head[anobucket].next; b != &bcache.head[anobucket]; b = b->next) {
      if (b->refcnt == 0 && (tmp == 0 || b->timestamp > tmp->timestamp)) {
        tmp = b;
      }
    }

    if (tmp != 0) {
      // release locks
      tmp->prev->next = tmp->next;
      tmp->next->prev = tmp->prev;
      tmp->dev = dev;
      tmp->blockno = blockno;
      tmp->valid = 0;
      tmp->refcnt = 1;
      release(&bcache.lock[anobucket]);
      break;
    }
    release(&bcache.lock[anobucket]);
  }
  if (tmp != 0) {
    release(&bcache.biglock);
    acquire(&bcache.lock[bn]);
    tmp->prev = &bcache.head[bn];
    tmp->next = bcache.head[bn].next;
    bcache.head[bn].next->prev = tmp;
    bcache.head[bn].next = tmp;
    acquiresleep(&tmp->lock);
    release(&bcache.lock[bn]);
    return tmp;
  }
  
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

  //acquire(&bcache.biglock);
  releasesleep(&b->lock);
  int bn = b->blockno % bucketN;

  acquire(&bcache.lock[bn]);
  b->refcnt--;
  b->timestamp = ticks;
  release(&bcache.lock[bn]);
  //release(&bcache.biglock);
}

void
bpin(struct buf *b) {
  int bn = b->blockno % bucketN;
  acquire(&bcache.lock[bn]);
  b->refcnt++;
  release(&bcache.lock[bn]);
}

void
bunpin(struct buf *b) {
  int bn = b->blockno % bucketN;
  acquire(&bcache.lock[bn]);
  b->refcnt--;
  release(&bcache.lock[bn]);
}


