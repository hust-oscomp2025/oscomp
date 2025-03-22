#include <kernel/fs/inode.h>
#include <kernel/mm/page.h>
#include <kernel/types.h>
#include <util/radix_tree.h>
#include <util/spinlock.h>

/*
 * Functions for managing the addrSpace structure and page cache
 */



/**
 * Initialize an addrSpace structure
 * @mapping: The addrSpace to initialize
 * @ops: Operations for this addrSpace
 *
 * This function initializes the radix tree, lock, and other fields
 */
void address_space_init(struct addrSpace* mapping, const struct addrSpace_ops* ops) {
	mapping->a_ops = ops;
	mapping->nrpages = 0;
	radix_tree_init(&mapping->page_tree);
	spinlock_init(&mapping->tree_lock);
}

struct addrSpace* addrSpace_create(struct inode* inode){


	
}



/**
 * Find a page in the addrSpace
 * @mapping: The addrSpace to search
 * @index: The page index to find
 *
 * Returns the page if found, NULL otherwise
 */
struct page* find_get_page(struct addrSpace* mapping, unsigned long index) {
	struct page* page;

	spinlock_lock(&mapping->tree_lock);
	page = radix_tree_lookup(&mapping->page_tree, index);
	if (page)
		get_page(page); /* Increment the reference count */
	spinlock_unlock(&mapping->tree_lock);

	return page;
}

/**
 * Add a page to the addrSpace
 * @mapping: The addrSpace
 * @page: The page to add
 * @index: The index at which to add the page
 *
 * Returns 0 on success, negative error code on failure
 */
int add_to_page_cache(struct addrSpace* mapping, struct page* page, unsigned long index) {
	int ret;

	get_page(page); /* Increment ref count before adding */

	spinlock_lock(&mapping->tree_lock);
	ret = radix_tree_insert(&mapping->page_tree, index, page);
	if (ret == 0) {
		page->mapping = mapping;
		page->index = index;
		mapping->nrpages++;
		radix_tree_tag_set(&mapping->page_tree, index, RADIX_TREE_TAG_ACCESSED);
	} else {
		put_page(page); /* Decrement ref count on failure */
	}
	spinlock_unlock(&mapping->tree_lock);

	return ret;
}

/**
 * Remove a page from the addrSpace
 * @mapping: The addrSpace
 * @page: The page to remove
 *
 * Returns 1 if the page was found and removed, 0 otherwise
 */
int remove_from_page_cache(struct addrSpace* mapping, struct page* page) {
	int ret = 0;

	spinlock_lock(&mapping->tree_lock);
	if (radix_tree_delete(&mapping->page_tree, page->index) == page) {
		mapping->nrpages--;
		page->mapping = NULL;
		ret = 1;
	}
	spinlock_unlock(&mapping->tree_lock);

	if (ret)
		put_page(page); /* Decrement ref count after removing */

	return ret;
}

/**
 * Mark a page dirty in the addrSpace
 * @mapping: The addrSpace
 * @page: The page to mark dirty
 *
 * Returns 1 if the page was successfully marked dirty, 0 otherwise
 */
int set_page_dirty_in_address_space(struct addrSpace* mapping, struct page* page) {
	if (!mapping || !page)
		return 0;

	spinlock_lock(&mapping->tree_lock);
	if (page->mapping == mapping) {
		set_page_dirty(page);
		radix_tree_tag_set(&mapping->page_tree, page->index, RADIX_TREE_TAG_DIRTY);
		spinlock_unlock(&mapping->tree_lock);
		return 1;
	}
	spinlock_unlock(&mapping->tree_lock);

	return 0;
}

/**
 * Find and get multiple dirty pages from the addrSpace
 * @mapping: The addrSpace to search
 * @pages: Array to store found pages
 * @nr_pages: Maximum number of pages to retrieve
 * @start: Starting index
 *
 * Returns the number of pages found
 */
unsigned int find_get_pages_dirty(struct addrSpace* mapping, struct page** pages, unsigned int nr_pages, unsigned long start) {
	unsigned int found;
	unsigned int i;

	spinlock_lock(&mapping->tree_lock);
	found = radix_tree_gang_lookup_tag(&mapping->page_tree, (void**)pages, start, nr_pages, RADIX_TREE_TAG_DIRTY);

	/* Increment reference counts for the found pages */
	for (i = 0; i < found; i++)
		get_page(pages[i]);

	spinlock_unlock(&mapping->tree_lock);

	return found;
}

/**
 * Clear the dirty tag from a page in the addrSpace
 * @mapping: The addrSpace
 * @page: The page to clear
 *
 * Returns 1 if successful, 0 otherwise
 */
int clear_page_dirty_in_address_space(struct addrSpace* mapping, struct page* page) {
	if (!mapping || !page)
		return 0;

	spinlock_lock(&mapping->tree_lock);
	if (page->mapping == mapping) {
		clear_page_dirty(page);
		radix_tree_tag_clear(&mapping->page_tree, page->index, RADIX_TREE_TAG_DIRTY);
		spinlock_unlock(&mapping->tree_lock);
		return 1;
	}
	spinlock_unlock(&mapping->tree_lock);

	return 0;
}

/**
 * Write back all dirty pages in the addrSpace
 * @mapping: The addrSpace to write back
 *
 * Returns 0 on success, negative error code on failure
 */
int write_back_address_space(struct addrSpace* mapping) {
	struct page* pages[16]; /* Process 16 pages in each batch */
	unsigned int nr_pages;
	unsigned long index = 0;
	int ret = 0;

	if (!mapping || !mapping->a_ops || !mapping->a_ops->writepage)
		return -EINVAL;

	/* Process batches of dirty pages until no more are found */
	do {
		unsigned int i;
		struct writeback_control wbc = {0};

		nr_pages = find_get_pages_dirty(mapping, pages, 16, index);

		for (i = 0; i < nr_pages; i++) {
			struct page* page = pages[i];

			/* Update the next index to search from */
			if (page->index > index)
				index = page->index;

			/* Skip this page if it's no longer dirty */
			if (!test_page_dirty(page)) {
				put_page(page);
				continue;
			}

			/* Lock the page for writeback */
			if (trylock_page(page)) {
				/* Write the page */
				ret = mapping->a_ops->writepage(page, &wbc);

				/* Clear the dirty tag if successful */
				if (ret == 0)
					clear_page_dirty_in_address_space(mapping, page);

				unlock_page(page);
			}

			put_page(page);

			if (ret < 0)
				break; /* Stop on error */
		}

		index++; /* Move to the next index */
	} while (nr_pages > 0 && ret == 0);

	return ret;
}

/**
 * Invalidate a single page in the addrSpace
 * @mapping: The addrSpace
 * @page: The page to invalidate
 *
 * Removes a page from the page cache if it's clean,
 * or fails if the page is dirty.
 *
 * Returns 0 on success, -EBUSY if the page is dirty
 */
int invalidate_page(struct addrSpace* mapping, struct page* page);
{
	int ret = 0;

	spinlock_lock(&mapping->tree_lock);

	/* Don't invalidate dirty pages */
	if (test_page_dirty(page)) {
		ret = -EBUSY;
	} else if (page->mapping == mapping) {
		/* Remove only if it's in this mapping */
		radix_tree_delete(&mapping->page_tree, page->index);
		mapping->nrpages--;
		page->mapping = NULL;
	}

	spinlock_unlock(&mapping->tree_lock);

	if (ret == 0)
		put_page(page); /* Decrement ref count after removing */

	return ret;
}

/**
 * Find or create a page at a specific index
 * @mapping: The addrSpace
 * @index: The index to find or create a page for
 * @gfp_mask: Allocation flags for new page
 *
 * Returns the found or created page, or NULL on failure
 */
struct page* find_or_create_page(struct addrSpace* mapping, unsigned long index, unsigned int gfp_mask) {
	struct page* page;

	/* First try to find the page */
	page = find_get_page(mapping, index);
	if (page)
		return page;

	/* Page not found, allocate a new one */
	page = alloc_page();
	if (!page)
		return NULL;

	/* Initialize the new page */
	page->mapping = NULL;
	page->flags = 0;
	atomic_set(&page->_refcount, 1);

	/* Try to add the page to the cache */
	if (add_to_page_cache(mapping, page, index) < 0) {
		/* Failed to add, might already exist now */
		put_page(page);

		/* Try to find it again */
		page = find_get_page(mapping, index);
	}

	return page;
}

/**
 * Read a page into the addrSpace at the specified index
 * @mapping: The addrSpace
 * @index: The index to read
 *
 * Returns the read page, or NULL on failure
 */
struct page* read_mapping_page(struct addrSpace* mapping, unsigned long index) {
	struct page* page;
	int ret;

	/* Try to find the page in the cache first */
	page = find_get_page(mapping, index);
	if (page) {
		if (page_uptodate(page))
			return page;

		/* Page exists but isn't up to date - need to read it */
		if (mapping->a_ops && mapping->a_ops->readpage) {
			struct file dummy_file = {.f_mapping = mapping};

			lock_page(page);
			ret = mapping->a_ops->readpage(&dummy_file, page);
			unlock_page(page);

			if (ret == 0 && page_uptodate(page))
				return page;
		}

		/* Reading failed or no readpage operation */
		put_page(page);
		return NULL;
	}

	/* Page not in cache, create a new one */
	page = find_or_create_page(mapping, index, 0);
	if (!page)
		return NULL;

	/* Read the page data */
	if (mapping->a_ops && mapping->a_ops->readpage) {
		struct file dummy_file = {.f_mapping = mapping};

		lock_page(page);
		ret = mapping->a_ops->readpage(&dummy_file, page);
		unlock_page(page);

		if (ret == 0 && page_uptodate(page))
			return page;
	}

	/* Reading failed */
	put_page(page);
	return NULL;
}