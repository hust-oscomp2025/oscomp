#ifndef _ADDRESS_SPACE_H
#define _ADDRESS_SPACE_H


#include <util/radix_tree.h>
#include <util/spinlock.h>

struct inode;

/* Memory management */
struct addrSpace {
	// struct inode *host;               /* Owning inode */
	struct radixTreeRoot page_tree;             /* Page cache radix tree */
	spinlock_t tree_lock;                         /* Lock for tree manipulation */
	unsigned long nrpages;                        /* Number of total pages */
	const struct addrSpace_ops* a_ops; /* s_operations */
};

struct addrSpace* addrSpace_create(struct inode* inode);

void address_space_init(struct addrSpace* mapping, const struct addrSpace_ops* ops);
struct page* find_get_page(struct addrSpace* mapping, unsigned long index);
int add_to_page_cache(struct addrSpace* mapping, struct page* page, unsigned long index);
int remove_from_page_cache(struct addrSpace *mapping, struct page *page);
int set_page_dirty_in_address_space(struct addrSpace *mapping, struct page *page);
unsigned int find_get_pages_dirty(struct addrSpace* mapping, struct page** pages, unsigned int nr_pages, unsigned long start);
int clear_page_dirty_in_address_space(struct addrSpace *mapping, struct page *page);
int write_back_address_space(struct addrSpace *mapping);
int invalidate_page(struct addrSpace *mapping, struct page *page);
struct page* find_or_create_page(struct addrSpace* mapping, unsigned long index, unsigned int gfp_mask);
struct page* read_mapping_page(struct addrSpace* mapping, unsigned long index);

/*
 * Address space s_operations (page cache)
 */
struct addrSpace_ops {
	int (*readpage)(struct file*, struct page*);
	int (*writepage)(struct page*, struct writeback_control*);
	int (*readpages)(struct file*, struct addrSpace*, struct list_head*, unsigned);
	int (*writepages)(struct addrSpace*, struct writeback_control*);
	void (*invalidatepage)(struct page*, unsigned int);
	int (*releasepage)(struct page*, int);
	int (*direct_IO)(int, struct kiocb*, const struct io_vector*, loff_t, unsigned long);
};

#endif