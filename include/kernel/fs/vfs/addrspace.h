#ifndef _ADDRESS_SPACE_H
#define _ADDRESS_SPACE_H


#include <kernel/util/radix_tree.h>
#include <kernel/util/spinlock.h>

struct inode;
struct writeback_control;
/* Memory management */
struct addrSpace {
	// struct inode *host;               /* Owning inode */
	struct radixTreeRoot page_tree;             /* Page cache radix tree */
	spinlock_t tree_lock;                         /* Lock for tree manipulation */
	uint64 nrpages;                        /* Number of total pages */
	const struct addrSpace_ops* a_ops; /* s_operations */
};

struct addrSpace* addrSpace_create(struct inode* inode);

struct page* addrSpace_getPage(struct addrSpace* mapping, uint64 index);
struct page* addrSpace_acquirePage(struct addrSpace* mapping, uint64 index, uint32 gfp_mask);
int32 addrSpace_addPage(struct addrSpace* mapping, struct page* page, uint64 index);
int32 addrSpace_putPage(struct addrSpace *mapping, struct page *page);
int32 addrSpace_setPageDirty(struct addrSpace *mapping, struct page *page);
uint32 addrSpace_getDirtyPages(struct addrSpace* mapping, struct page** pages, uint32 nr_pages, uint64 start);
int32 addrSpace_removeDirtyTag(struct addrSpace *mapping, struct page *page);
int32 addrSpace_writeBack(struct addrSpace *mapping);
int32 addrSpace_writeback_range(struct addrSpace *mapping, loff_t start, loff_t end, int32 sync_mode);
int32 addrSpace_invalidate(struct addrSpace *mapping, struct page *page);

struct page* addrSpace_readPage(struct addrSpace* mapping, uint64 index);



#endif