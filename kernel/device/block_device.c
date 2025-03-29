#include <kernel/device/block_device.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/types.h>
#include <kernel/util.h>
#include <kernel/types.h>
#include <kernel/sprint.h>

/* Global block device list */
struct list_head block_device_list;
spinlock_t block_device_list_lock;

/* Major number mapping table */
#define MAX_MAJOR 256
#define DYNAMIC_MAJOR_MIN 128
static struct block_operations *blk_majors[MAX_MAJOR];
static spinlock_t major_lock;
static char *major_names[MAX_MAJOR];

/**
 * alloc_block_device - Allocate a new block device structure
 *
 * Returns: Newly allocated block device or NULL
 */
struct block_device *alloc_block_device(void) {
    struct block_device *bdev = kmalloc(sizeof(struct block_device));
    if (!bdev)
        return NULL;
    
    memset(bdev, 0, sizeof(struct block_device));
    atomic_set(&bdev->bd_refcnt, 1);  /* Initial reference count */
    spinlock_init(&bdev->bd_lock);
    INIT_LIST_HEAD(&bdev->bd_list);
    
    return bdev;
}

/**
 * free_block_device - Free block device structure 
 * @bdev: Block device to free
 */
void free_block_device(struct block_device *bdev) {
    if (!bdev)
        return;
    
    if (atomic_read(&bdev->bd_refcnt) > 0) {
        sprint("Warning: freeing block device with active references\n");
    }
    
    kfree(bdev);
}

/**
 * blockdevice_lookup - Get a reference to a block device
 * @dev: Device number
 *
 * This function finds the block device by device number and
 * increases its reference count, but does not open it.
 * 
 * Returns: Block device pointer or NULL if not found
 */
struct block_device *blockdevice_lookup(dev_t dev) {
    struct block_device *bdev = NULL;
    
    spinlock_lock(&block_device_list_lock);
    list_for_each_entry(bdev, &block_device_list, bd_list) {
        if (bdev->bd_dev == dev) {
            atomic_inc(&bdev->bd_refcnt);
            spinlock_unlock(&block_device_list_lock);
            return bdev;
        }
    }
    spinlock_unlock(&block_device_list_lock);
    
    return NULL;
}

/**
 * blockdevice_unref - Release a reference to a block device
 * @bdev: Block device to release
 *
 * Decreases reference count and frees the structure when it reaches zero
 */
void blockdevice_unref(struct block_device *bdev) {
    if (!bdev)
        return;
    
    if (atomic_dec_and_test(&bdev->bd_refcnt)) {
        /* Last reference - remove from list and free */
        spinlock_lock(&block_device_list_lock);
        list_del(&bdev->bd_list);
        spinlock_unlock(&block_device_list_lock);
        
        free_block_device(bdev);
    }
}

/**
 * blockdevice_open - Open a block device
 * @bdev: Block device to open
 * @mode: File mode flags (O_RDONLY, O_WRONLY, etc.)
 *
 * This opens the block device for use, calling its open method if available.
 * 
 * Returns: 0 on success, error code on failure
 */
int32 blockdevice_open(struct block_device *bdev, fmode_t mode) {
    int32 res = 0;
    uint32 access_mode = mode & O_ACCMODE;
    
    if (!bdev)
        return -EINVAL;
    
    spinlock_lock(&bdev->bd_lock);
    
    /* Call device open method if available */
    if (bdev->bd_ops && bdev->bd_ops->open)
        res = bdev->bd_ops->open(bdev, access_mode);
    
    if (res == 0) {
        /* Track open mode and opener count */
        bdev->bd_mode |= access_mode;
        bdev->bd_openers++;
    }
    
    spinlock_unlock(&bdev->bd_lock);
    return res;
}

/**
 * blockdevice_close - Close a block device
 * @bdev: Block device to close
 *
 * This closes the block device, calling its release method if needed.
 */
void blockdevice_close(struct block_device *bdev) {
    if (!bdev)
        return;
    
    spinlock_lock(&bdev->bd_lock);
    
    /* Only call release when all openers are gone */
    if (bdev->bd_openers > 0) {
        bdev->bd_openers--;
        
        if (bdev->bd_openers == 0) {
            /* Last opener - call release */
            if (bdev->bd_ops && bdev->bd_ops->release) {
                /* Call outside of lock */
                spinlock_unlock(&bdev->bd_lock);
                bdev->bd_ops->release(bdev);
                spinlock_lock(&bdev->bd_lock);
            }
            
            /* Clear mode flags */
            bdev->bd_mode = 0;
        }
    }
    
    spinlock_unlock(&bdev->bd_lock);
}