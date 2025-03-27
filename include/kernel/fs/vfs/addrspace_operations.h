#pragma once
#include <kernel/vfs.h>


/*
 * Address space s_operations (page cache)
 */
struct addrSpace_ops {
	int32 (*readpage)(struct file*, struct page*);
	int32 (*writepage)(struct page*, struct writeback_control*);
	int32 (*readpages)(struct file*, struct addrSpace*, struct list_head*, unsigned);
	int32 (*writepages)(struct addrSpace*, struct writeback_control*);
	void (*invalidatepage)(struct page*, uint32);
	int32 (*releasepage)(struct page*, int32);
	int32 (*direct_IO)(int32, struct kiocb*, const struct io_vector*, loff_t, uint64);
};