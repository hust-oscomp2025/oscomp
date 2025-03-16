#ifndef _ADDRESS_SPACE_H
#define _ADDRESS_SPACE_H

#include <kernel/types.h>
#include <util/atomic.h>
#include <util/spinlock.h>

#include <kernel/riscv.h>
#include <util/list.h>
#include <kernel/mm/page.h>

// 前向声明，避免循环引用
struct inode;
struct address_space_operations;

/*
 * Address space (page cache)
 */
struct address_space {
	struct inode *host;               /* Owning inode */
	struct radix_tree_root page_tree; /* Page cache radix tree */
	spinlock_t tree_lock;             /* Lock for tree manipulation */
	unsigned long nrpages;            /* Number of total pages */
	const struct address_space_operations *a_ops;  /* Operations */
};

/*
 * Address space operations (page cache)
 */
struct address_space_operations {
	int (*readpage)(struct file *, struct page *);
	int (*writepage)(struct page *, struct writeback_control *);
	int (*readpages)(struct file *, struct address_space *, struct list_head *, unsigned);
	int (*writepages)(struct address_space *, struct writeback_control *);
	void (*invalidatepage)(struct page *, unsigned int);
	int (*releasepage)(struct page *, int);
	int (*direct_IO)(int, struct kiocb *, const struct iovec *, loff_t, unsigned long);
};

#endif /* _ADDRESS_SPACE_H */