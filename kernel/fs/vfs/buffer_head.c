/**
 * @file buffer_head.c
 * @brief Implementation of buffer_head functions
 *
 * This file implements the functions declared in buffer_head.h for
 * managing buffer heads that represent disk blocks in memory.
 */

#include <errno.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/types.h>
#include <string.h>

/* I/O operation types */
#define READ 0
#define WRITE 1

/**
 * Initialize the buffer_head subsystem
 */
int buffer_head_init(void) {
	/* No specialized initialization needed when using kmalloc */
	return 0;
}

/**
 * Initialize a buffer_head structure
 *
 * @param bh The buffer_head to initialize
 */
void init_buffer_head(struct buffer_head* bh) {
	if (!bh)
		return;

	memset(bh, 0, sizeof(struct buffer_head));

	/* Initialize the fields */
	bh->b_state = 0;
	bh->b_this_page = NULL;
	bh->b_page = NULL;
	bh->b_blocknr = 0;
	bh->b_size = 0;
	bh->b_data = NULL;
	bh->b_bdev = NULL;

	atomic_set(&bh->b_count, 0);
	spin_lock_init(&bh->b_uptodate_lock);
	bh->b_end_io = NULL;
	bh->b_private = NULL;

	INIT_LIST_HEAD(&bh->b_assoc_buffers);
	INIT_LIST_HEAD(&bh->b_lru);
}

/**
 * Allocate a new buffer_head
 *
 * @return Pointer to newly allocated buffer_head or NULL on failure
 */
struct buffer_head* alloc_buffer_head(void) {
	struct buffer_head* bh;

	// bh = kmalloc(sizeof(struct buffer_head), GFP_KERNEL);
	bh = kmalloc(sizeof(struct buffer_head));

	if (!bh)
		return NULL;

	init_buffer_head(bh);
	atomic_set(&bh->b_count, 1);

	return bh;
}

/**
 * Free a buffer_head structure
 *
 * @param bh The buffer_head to free
 */
void free_buffer_head(struct buffer_head* bh) {
	if (!bh)
		return;

	/* Make sure the buffer is not in use */
	if (atomic_read(&bh->b_count) != 0) {
		// printk(KERN_ERR "Warning: Freeing buffer with non-zero count\n");
		return;
	}

	/* Remove from any lists */
	list_del(&bh->b_assoc_buffers);
	list_del(&bh->b_lru);

	kfree(bh);
}

/**
 * Get a reference to a buffer_head
 *
 * @param bh The buffer_head to get a reference to
 * @return The same buffer_head with its reference count incremented
 */
struct buffer_head* get_buffer_head(struct buffer_head* bh) {
	if (!bh)
		return NULL;

	atomic_inc(&bh->b_count);
	return bh;
}

/**
 * Release a reference to a buffer_head
 *
 * @param bh The buffer_head to release
 */
void put_buffer_head(struct buffer_head* bh) {
	if (!bh)
		return;

	if (atomic_dec_and_test(&bh->b_count))
		free_buffer_head(bh);
}

/**
 * Mark a buffer_head as dirty
 *
 * @param bh The buffer_head to mark dirty
 */
void mark_buffer_dirty(struct buffer_head* bh) {
	if (!bh)
		return;

	/* Set the dirty bit */
	set_bit(BH_Dirty, &bh->b_state);

	/* If the buffer is associated with a page, mark the page as dirty too */
	if (bh->b_page)
		set_page_dirty(bh->b_page);
}

/**
 * Mark a buffer_head as clean
 *
 * @param bh The buffer_head to mark clean
 */
void mark_buffer_clean(struct buffer_head* bh) {
	if (!bh)
		return;

	/* Clear the dirty bit */
	clear_bit(BH_Dirty, &bh->b_state);
}

/**
 * Lock a buffer_head for I/O
 *
 * @param bh The buffer_head to lock
 */
void lock_buffer(struct buffer_head* bh) {
	if (!bh)
		return;

	/* Set the lock bit, waiting if it's already set */
	while (test_and_set_bit(BH_Lock, &bh->b_state)) {
		wait_on_bit(&bh->b_state, BH_Lock, TASK_UNINTERRUPTIBLE);
	}
}

/**
 * Unlock a buffer_head
 *
 * @param bh The buffer_head to unlock
 */
void unlock_buffer(struct buffer_head* bh) {
	if (!bh)
		return;

	/* Clear the lock bit and wake up any waiters */
	clear_bit_unlock(BH_Lock, &bh->b_state);
	wake_up_bit(&bh->b_state, BH_Lock);
}

/**
 * Wait for a buffer_head to complete I/O
 *
 * @param bh The buffer_head to wait for
 */
void wait_on_buffer(struct buffer_head* bh) {
	if (!bh)
		return;

	/* Wait for the lock bit to be cleared */
	wait_on_bit(&bh->b_state, BH_Lock, TASK_UNINTERRUPTIBLE);
}

/**
 * Read a buffer synchronously
 *
 * @param bh The buffer_head to read
 * @return 0 on success, negative error code on failure
 */
int sync_read_buffer(struct buffer_head* bh) {
	int ret = 0;

	if (!bh || !bh->b_bdev)
		return -EINVAL;

	/* Lock the buffer */
	lock_buffer(bh);

	/* If the buffer is already up to date, we can skip the read */
	if (buffer_uptodate(bh)) {
		unlock_buffer(bh);
		return 0;
	}

	/* Read the block from the device */
	ret = bdev_read_block(bh->b_bdev, bh->b_blocknr, bh->b_data, bh->b_size);

	if (ret == 0) {
		/* Mark the buffer as up to date */
		set_bit(BH_Uptodate, &bh->b_state);
	}

	/* Unlock the buffer */
	unlock_buffer(bh);

	return ret;
}

/**
 * Write a buffer synchronously
 *
 * @param bh The buffer_head to write
 * @return 0 on success, negative error code on failure
 */
int sync_write_buffer(struct buffer_head* bh) {
	int ret = 0;

	if (!bh || !bh->b_bdev)
		return -EINVAL;

	/* Lock the buffer */
	lock_buffer(bh);

	/* If the buffer is not dirty, we can skip the write */
	if (!buffer_dirty(bh)) {
		unlock_buffer(bh);
		return 0;
	}

	/* Write the block to the device */
	ret = bdev_write_block(bh->b_bdev, bh->b_blocknr, bh->b_data, bh->b_size);

	if (ret == 0) {
		/* Mark the buffer as clean */
		clear_bit(BH_Dirty, &bh->b_state);
	}

	/* Unlock the buffer */
	unlock_buffer(bh);

	return ret;
}

/**
 * Completion callback for asynchronous I/O
 */
static void end_buffer_async_op(struct buffer_head* bh, int uptodate) {
	/* Mark the buffer as up to date if the operation succeeded */
	if (uptodate)
		set_bit(BH_Uptodate, &bh->b_state);

	/* If a completion function was provided, call it */
	if (bh->b_end_io)
		bh->b_end_io(bh, uptodate);

	/* Unlock the buffer */
	unlock_buffer(bh);

	/* Release our reference */
	put_buffer_head(bh);
}

/**
 * Submit a buffer for asynchronous I/O
 *
 * @param op I/O operation (read or write)
 * @param bh The buffer_head to submit
 * @param end_io Completion function
 * @return 0 on success, negative error code on failure
 */
int submit_bh(int op, struct buffer_head* bh, bh_end_io_t end_io) {
	int ret = 0;

	if (!bh || !bh->b_bdev)
		return -EINVAL;

	/* Get a reference for the I/O operation */
	get_buffer_head(bh);

	/* Lock the buffer */
	lock_buffer(bh);

	/* Set the completion function */
	bh->b_end_io = end_io;

	/* Set the appropriate state bits */
	if (op == READ) {
		set_bit(BH_Async_Read, &bh->b_state);
	} else {
		set_bit(BH_Async_Write, &bh->b_state);
	}

	/* Submit the I/O request */
	if (op == READ) {
		ret = bdev_read_block_async(bh->b_bdev, bh->b_blocknr, bh->b_data, bh->b_size, end_buffer_async_op, bh);
	} else {
		ret = bdev_write_block_async(bh->b_bdev, bh->b_blocknr, bh->b_data, bh->b_size, end_buffer_async_op, bh);
	}

	if (ret != 0) {
		/* I/O submission failed, clean up */
		clear_bit(BH_Async_Read, &bh->b_state);
		clear_bit(BH_Async_Write, &bh->b_state);
		unlock_buffer(bh);
		put_buffer_head(bh);
	}

	return ret;
}

/**
 * Create and map a buffer for a given device block
 *
 * @param dev Block device
 * @param block Block number
 * @param size Block size
 * @return Mapped buffer_head or NULL on failure
 */
struct buffer_head* map_bh(struct block_device* dev, sector_t block, size_t size) {
	struct buffer_head* bh;
	struct page* page;

	if (!dev)
		return NULL;

	/* Allocate a buffer head */
	bh = alloc_buffer_head();
	if (!bh)
		return NULL;

	/* Allocate a page for the data */
	page = alloc_page();
	if (!page) {
		free_buffer_head(bh);
		return NULL;
	}

	/* Set up the buffer head */
	bh->b_bdev = dev;
	bh->b_blocknr = block;
	bh->b_size = size;
	bh->b_page = page;
	bh->b_data = page_address(page);

	/* Mark the buffer as mapped */
	set_bit(BH_Mapped, &bh->b_state);

	return bh;
}

/**
 * Read block from a file
 *
 * @param inode Target inode
 * @param block Logical block number
 * @param bh Buffer head to fill
 * @param create Create block if it doesn't exist
 * @return 0 on success, negative error code on failure
 */
int get_block(struct inode* inode, sector_t block, struct buffer_head* bh, int create) {
	int ret;
	sector_t phys_block;

	if (!inode || !bh)
		return -EINVAL;

	/* Call the filesystem-specific get_block function */
	ret = inode->i_superblock->s_operations->get_block(inode, block, bh, create);

	return ret;
}

/**
 * Cleanup for the buffer_head subsystem
 */
void buffer_head_exit(void) { /* No cleanup needed when using kmalloc/kfree */ }