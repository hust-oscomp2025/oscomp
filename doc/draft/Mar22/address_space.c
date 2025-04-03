#include <kernel/fs/inode.h>
#include <kernel/mm/page.h>
#include <util/address_space.h>
#include <util/radix_tree.h>

/**
 * Generic implementation for readpage
 * This function serves as a generic implementation that can be used
 * by filesystems that don't provide their own readpage method.
 */
int generic_readpage(struct file *file, struct page *page)
{
    struct inode *inode = file->f_inode;
    struct address_space *mapping = inode->i_mapping;
    loff_t pos = page->index << PAGE_SHIFT;
    ssize_t bytes_read;
    
    /* Allocate a buffer for the page data if needed */
    if (!page->paddr) {
        struct page *new_page = alloc_page();
        if (!new_page)
            return -ENOMEM;
        
        /* Copy the page properties */
        page->paddr = new_page->paddr;
        put_page(new_page);  /* We only want the physical memory */
    }
    
    /* Set up kiocb for reading */
    struct kiocb kiocb;
    init_kiocb(&kiocb, file);
    kiocb_set_pos(&kiocb, pos);
    
    /* Read the data */
    bytes_read = kiocb_read(&kiocb, (char *)page->paddr, PAGE_SIZE);
    
    if (bytes_read < 0) {
        /* Read error */
        return bytes_read;
    }
    
    /* If we read less than a full page, zero the rest */
    if (bytes_read < PAGE_SIZE) {
        memset((char *)page->paddr + bytes_read, 0, PAGE_SIZE - bytes_read);
    }
    
    /* Mark the page as up-to-date */
    set_page_uptodate(page);
    
    return 0;
}

/**
 * Generic implementation for writepage
 * This function serves as a generic implementation that can be used
 * by filesystems that don't provide their own writepage method.
 */
int generic_writepage(struct page *page, struct writeback_control *wbc)
{
    struct address_space *mapping = page->mapping;
    struct inode *inode;
    struct file file;
    loff_t pos;
    ssize_t bytes_written;
    
    if (!mapping)
        return -EINVAL;
    
    inode = container_of(mapping, struct inode, i_mapping);
    
    /* Set up a temporary file for writing */
    file.f_inode = inode;
    file.f_mapping = mapping;
    file.f_operations = inode->i_fop;
    
    /* Position to write from */
    pos = page->index << PAGE_SHIFT;
    
    /* Set up kiocb for writing */
    struct kiocb kiocb;
    init_kiocb(&kiocb, &file);
    kiocb_set_pos(&kiocb, pos);
    
    /* Write the data */
    bytes_written = kiocb_write(&kiocb, (const char *)page->paddr, PAGE_SIZE);
    
    if (bytes_written < 0) {
        /* Write error */
        return bytes_written;
    }
    
    /* Update inode size if necessary */
    loff_t new_size = pos + bytes_written;
    if (new_size > inode->i_size) {
        inode->i_size = new_size;
        mark_inode_dirty(inode);
    }
    
    return 0;
}

/**
 * Read multiple pages at once
 * More efficient than reading individual pages when we know we'll need
 * a range of pages.
 */
int read_pages(struct address_space *mapping, struct file *file,
              unsigned long start, unsigned long nr_pages,
              struct page **pages)
{
    unsigned long i;
    int ret = 0;
    
    if (!mapping->a_ops || !mapping->a_ops->readpage)
        return -EINVAL;
    
    for (i = 0; i < nr_pages; i++) {
        if (!pages[i])
            continue;
            
        ret = mapping->a_ops->readpage(file, pages[i]);
        if (ret)
            break;
    }
    
    return ret;
}

/**
 * Write multiple pages at once
 */
int write_pages(struct address_space *mapping,
               struct page **pages, unsigned long nr_pages,
               struct writeback_control *wbc)
{
    unsigned long i;
    int ret = 0;
    
    if (!mapping->a_ops || !mapping->a_ops->writepage)
        return -EINVAL;
    
    for (i = 0; i < nr_pages; i++) {
        if (!pages[i] || !test_page_dirty(pages[i]))
            continue;
            
        ret = mapping->a_ops->writepage(pages[i], wbc);
        if (ret)
            break;
    }
    
    return ret;
}

/**
 * Release a page from the address_space
 * Called when a page's reference count reaches zero
 */
int release_page(struct address_space *mapping, struct page *page)
{
    int ret = 0;
    
    if (!mapping || !page)
        return -EINVAL;
    
    spinlock_lock(&mapping->tree_lock);
    
    /* If the page is still dirty, we can't release it yet */
    if (test_page_dirty(page)) {
        ret = -EBUSY;
    } else if (page->mapping == mapping) {
        /* Remove from the radix tree */
        radix_tree_delete(&mapping->page_tree, page->index);
        mapping->nrpages--;
        page->mapping = NULL;
    }
    
    spinlock_unlock(&mapping->tree_lock);
    
    return ret;
}

/**
 * Invalidate all pages in an address_space
 * Typically called when an inode is being evicted or a file is truncated
 */
int invalidate_mapping_pages(struct address_space *mapping)
{
    struct page *pages[16];  /* Process 16 pages in each batch */
    unsigned int nr_pages;
    unsigned long index = 0;
    int ret = 0;
    int i;
    
    if (!mapping)
        return -EINVAL;
    
    /* Keep going until we've processed all pages */
    do {
        spinlock_lock(&mapping->tree_lock);
        nr_pages = radix_tree_gang_lookup(&mapping->page_tree, 
                                      (void **)pages, index, 16);
        
        /* Try to lock each page for invalidation */
        for (i = 0; i < nr_pages; i++) {
            struct page *page = pages[i];
            
            /* Update the next index to search from */
            if (page->index > index)
                index = page->index;
            
            /* Skip dirty pages */
            if (test_page_dirty(page))
                continue;
            
            /* Remove from the radix tree */
            radix_tree_delete(&mapping->page_tree, page->index);
            mapping->nrpages--;
            page->mapping = NULL;
            
            /* Decrement reference count */
            put_page(page);
        }
        
        spinlock_unlock(&mapping->tree_lock);
        
        index++;  /* Move to the next index */
    } while (nr_pages > 0);
    
    return ret;
}

/**
 * Sync all dirty pages in an address_space
 */
int sync_mapping_pages(struct address_space *mapping, int wait)
{
    struct page *pages[16];  /* Process 16 pages in each batch */
    unsigned int nr_pages;
    unsigned long index = 0;
    int ret = 0;
    
    if (!mapping || !mapping->a_ops || !mapping->a_ops->writepage)
        return -EINVAL;
    
    /* Process batches of dirty pages until no more are found */
    do {
        unsigned int i;
        struct writeback_control wbc = { 
            .sync_mode = wait ? WB_SYNC_ALL : WB_SYNC_NONE 
        };
        
        nr_pages = find_get_pages_dirty(mapping, pages, 16, index);
        
        for (i = 0; i < nr_pages; i++) {
            struct page *page = pages[i];
            
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
            
            if (ret < 0 && wait)
                break;  /* Stop on error if we're in wait mode */
        }
        
        index++;  /* Move to the next index */
    } while (nr_pages > 0 && (ret == 0 || !wait));
    
    return ret;
}