#ifndef _BUFFER_HEAD_H
#define _BUFFER_HEAD_H
#include <kernel/device/block_device.h>
#include <kernel/util/list.h>
#include <kernel/util/spinlock.h>
#include <kernel/types.h>
#include <kernel/util/atomic.h>

/* Buffer states (b_state) */
enum {
    BH_Uptodate = 0,    /* Buffer contains valid data */
    BH_Dirty = 1,       /* Buffer is dirty */
    BH_Lock = 2,        /* Buffer is locked */
    BH_Req = 3,         /* Buffer has been requested */
    BH_Mapped = 4,      /* Buffer is mapped to disk */
    BH_New = 5,         /* Buffer is new and not yet allocated */
    BH_Async_Write = 6, /* Buffer is under async write */
    BH_Async_Read = 7,  /* Buffer is under async read */
    BH_Delay = 8,       /* Buffer is delayed allocation */
    BH_Boundary = 9,    /* Has disk mapping boundary */
    
    /* Additional states can be added as needed */
    BH_PrivateStart = 16/* First bit available for filesystems */
};

/* Buffer head - tracks a single block in the buffer cache */
struct buffer_head {
    struct buffer_head *b_this_page;   /* Circular list of buffers in page */
    struct block_device *b_bdev;       /* Block device this buffer is from */
    sector_t            b_blocknr;     /* Block number */
    size_t              b_size;        /* Block size */
    uint64       b_state;       /* State flags */
    atomic_t            b_count;       /* Reference counter */
    
    char               *b_data;        /* Pointer to data block */
    
    struct list_head    b_lru;         /* LRU list entry */
    spinlock_t          b_lock;        /* Buffer lock */
    
    /* 以下成员是可选的，如果需要，可以稍后实现 */
    
    uint64       b_end_io;      /* Completion function callback */
    void               *b_private;     /* Reserved for b_end_io */
    struct list_head    b_assoc_buffers; /* Associated buffer operations */
    struct address_space *b_assoc_map; /* Associated address space */
    atomic_t           b_tracked;      /* Remote locking protocol tracking */
    struct page        *b_page;        /* The page this buffer is mapped to */

};

/* Buffer state testing/setting/clearing functions */
#define buffer_uptodate(bh) test_bit(BH_Uptodate, &(bh)->b_state)
#define buffer_dirty(bh)    test_bit(BH_Dirty,    &(bh)->b_state)
#define buffer_locked(bh)   test_bit(BH_Lock,     &(bh)->b_state)
#define buffer_mapped(bh)   test_bit(BH_Mapped,   &(bh)->b_state)
#define buffer_new(bh)      test_bit(BH_New,      &(bh)->b_state)
#define buffer_delay(bh)    test_bit(BH_Delay,    &(bh)->b_state)

/* Set bits */
#define set_buffer_uptodate(bh)  set_bit(BH_Uptodate, &(bh)->b_state)
#define set_buffer_dirty(bh)     set_bit(BH_Dirty,    &(bh)->b_state)
#define set_buffer_locked(bh)    set_bit(BH_Lock,     &(bh)->b_state)
#define set_buffer_mapped(bh)    set_bit(BH_Mapped,   &(bh)->b_state)
#define set_buffer_new(bh)       set_bit(BH_New,      &(bh)->b_state)
#define set_buffer_delay(bh)     set_bit(BH_Delay,    &(bh)->b_state)

/* Clear bits */
#define clear_buffer_uptodate(bh) clear_bit(BH_Uptodate, &(bh)->b_state)
#define clear_buffer_dirty(bh)    clear_bit(BH_Dirty,    &(bh)->b_state)
#define clear_buffer_locked(bh)   clear_bit(BH_Lock,     &(bh)->b_state)
#define clear_buffer_mapped(bh)   clear_bit(BH_Mapped,   &(bh)->b_state)
#define clear_buffer_new(bh)      clear_bit(BH_New,      &(bh)->b_state)
#define clear_buffer_delay(bh)    clear_bit(BH_Delay,    &(bh)->b_state)

/* Essential functions */
/* 获取一个缓冲区，不读取数据 */
struct buffer_head *getblk(struct block_device *bdev, sector_t block, size_t size);

/* 获取一个缓冲区，并读取数据 */
struct buffer_head *bread(struct block_device *bdev, sector_t block, size_t size);

/* 释放对缓冲区的引用 */
void brelse(struct buffer_head *bh);

/* 标记缓冲区为脏 */
void mark_buffer_dirty(struct buffer_head *bh);

/* 以下是对ext4支持有用的附加函数 */

/* 提交一个缓冲区的内容 */
int32 sync_dirty_buffer(struct buffer_head *bh);

/* 异步读取一个缓冲区 */
void ll_rw_block(int32 rw, int32 nr, struct buffer_head *bhs[]);

/* 等待缓冲区操作完成 */
void wait_on_buffer(struct buffer_head *bh);

/* 锁住一个缓冲区 */
void lock_buffer(struct buffer_head *bh);

/* 解锁一个缓冲区 */
void unlock_buffer(struct buffer_head *bh);

/* 分配一个新的缓冲区 */
//struct buffer_head *alloc_buffer_head(gfp_t gfp_flags);
struct buffer_head *alloc_buffer_head(void);

/* 释放一个缓冲区 */
void free_buffer_head(struct buffer_head *bh);

/* 以下是可选功能，可以后续实现 */
/*
// 从页面映射获取缓冲区
struct buffer_head *page_buffers(struct page *page);

// 检查页面是否有缓冲区
int32 page_has_buffers(struct page *page);

// 创建页面缓冲区
int32 create_empty_buffers(struct page *page, uint64 blocksize, uint64 b_state);

// 映射缓冲区
void map_bh(struct buffer_head *bh, struct super_block *sb, sector_t block);

// 获取块映射
sector_t bmap(struct inode *inode, sector_t block);

// 缓冲同步函数组
int32 sync_blockdev(struct block_device *bdev);
*/

#endif /* _BUFFER_HEAD_H */