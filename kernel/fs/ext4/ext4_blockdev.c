#include <kernel/device/block_device.h>
#include <kernel/fs/lwext4/ext4_blockdev.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/types.h>
#include <kernel/util/string.h>
#include <kernel/vfs.h>

static inline struct block_device* __ext4_get_kernel_bdev(struct ext4_blockdev* e_blockdevice);


/* Forward declarations */
static int32 ext4_blockdev_adapter_open(struct ext4_blockdev* e_blockdevice);
static int32 ext4_blockdev_adapter_bread(struct ext4_blockdev* e_blockdevice, void* buf, uint64_t blk_id, uint32_t blk_cnt);
static int32 ext4_blockdev_adapter_bwrite(struct ext4_blockdev* e_blockdevice, const void* buf, uint64_t blk_id, uint32_t blk_cnt);
static int32 ext4_blockdev_adapter_close(struct ext4_blockdev* e_blockdevice);

struct ext4_blockdev* ext4_blockdev_create_adapter(struct block_device* kernel_bdev) {
	if (!kernel_bdev) return NULL;

	/* Allocate the ext4 blockdev structure */
	struct ext4_blockdev* e_blockdevice = kmalloc(sizeof(struct ext4_blockdev));
	if (!e_blockdevice) return NULL;

	/* Allocate the interface structure */
	struct ext4_blockdev_iface* iface = kmalloc(sizeof(struct ext4_blockdev_iface));
	if (!iface) {
		kfree(e_blockdevice);
		return NULL;
	}

	/* Initialize the interface structure */
	memset(iface, 0, sizeof(struct ext4_blockdev_iface));

	/* Set up the interface function pointers */
	iface->open = ext4_blockdev_adapter_open;
	iface->close = ext4_blockdev_adapter_close;
	iface->bread = ext4_blockdev_adapter_bread;
	iface->bwrite = ext4_blockdev_adapter_bwrite;

	/* Allocate a physical buffer for block operations */
	uint32_t block_size = kernel_bdev->bd_block_size ? kernel_bdev->bd_block_size : 4096;

	iface->ph_bbuf = kmalloc(block_size);
	if (!iface->ph_bbuf) {
		kfree(iface);
		kfree(e_blockdevice);
		return NULL;
	}

	/* Set up physical device parameters */

	iface->ph_bsize = block_size;

	iface->ph_bcnt = kernel_bdev->bd_nr_blocks; /* This should be available in your block_device */
	iface->ph_refctr = 1;                       /* Start with one reference */
	iface->bread_ctr = 0;
	iface->bwrite_ctr = 0;
	iface->p_user = kernel_bdev; /* Store kernel block device in user data */

	/* Initialize the blockdev structure */
	memset(e_blockdevice, 0, sizeof(struct ext4_blockdev));
	e_blockdevice->bdif = iface;
	e_blockdevice->part_offset = 0; /* No partition offset by default */
	e_blockdevice->part_size = iface->ph_bcnt * iface->ph_bsize;
	e_blockdevice->bc = NULL;             /* Cache will be set up later */
	e_blockdevice->lg_bsize = block_size; /* Start with same as physical */
	e_blockdevice->lg_bcnt = iface->ph_bcnt;
	e_blockdevice->cache_write_back = 0;
	e_blockdevice->fs = NULL; /* Will be set later */
	e_blockdevice->journal = NULL;

	return e_blockdevice;
}

void ext4_blockdev_free_adapter(struct ext4_blockdev* e_blockdevice) {
	if (e_blockdevice) {
		/* Close the device if it's open */
		if (e_blockdevice->bdif) {
			if (e_blockdevice->bdif->ph_bbuf) { kfree(e_blockdevice->bdif->ph_bbuf); }
			kfree(e_blockdevice->bdif);
		}

		kfree(e_blockdevice);
	}
}
/**
 * Adapter open function
 */
static int32 ext4_blockdev_adapter_open(struct ext4_blockdev* e_blockdevice) {
	int32 ret;

	if (!e_blockdevice) return -EINVAL;
	struct block_device* kernel_bdev = __ext4_get_kernel_bdev(e_blockdevice);
	if (!kernel_bdev) return -EINVAL;

	/* Use kernel's block device open function */
	if (kernel_bdev->bd_ops && kernel_bdev->bd_ops->open) {
		ret = kernel_bdev->bd_ops->open(kernel_bdev, FMODE_READ | FMODE_WRITE);
		if (ret != 0) return ret;
	}

	/* Mark as initialized */
	// e_blockdevice->flags |= EXT4_BDEV_INITIALIZED;

	return 0;
}

/**
 * Adapter block read function
 */
static int32 ext4_blockdev_adapter_bread(struct ext4_blockdev* e_blockdevice, void* buf, uint64_t blk_id, uint32_t blk_cnt) {
	

	if (!e_blockdevice || !buf) return -EINVAL;
	struct block_device* kernel_bdev = __ext4_get_kernel_bdev(e_blockdevice);

	if (!kernel_bdev) return -EINVAL;

	/* Use kernel's block device read function */
	if (kernel_bdev->bd_ops && kernel_bdev->bd_ops->read_blocks) { return kernel_bdev->bd_ops->read_blocks(kernel_bdev,buf, blk_id,  blk_cnt); }

	return -ENOSYS;
}

/**
 * Adapter block write function
 */
static int32 ext4_blockdev_adapter_bwrite(struct ext4_blockdev* e_blockdevice, const void* buf, uint64_t blk_id, uint32_t blk_cnt) {

	if (!e_blockdevice || !buf) return -EINVAL;
	struct block_device* kernel_bdev = __ext4_get_kernel_bdev(e_blockdevice);
	if (!kernel_bdev) return -EINVAL;

	/* Use kernel's block device write function */
	if (kernel_bdev->bd_ops && kernel_bdev->bd_ops->write_blocks) { return kernel_bdev->bd_ops->write_blocks(kernel_bdev,(void*)buf, blk_id, blk_cnt ); }

	return -ENOSYS;
}

/**
 * Adapter close function
 */
static int32 ext4_blockdev_adapter_close(struct ext4_blockdev* e_blockdevice) {
	

	if (!e_blockdevice) return -EINVAL;

	struct block_device* kernel_bdev = __ext4_get_kernel_bdev(e_blockdevice);

	if (!kernel_bdev) return -EINVAL;

	/* Use kernel's block device close function */
	if (kernel_bdev->bd_ops && kernel_bdev->bd_ops->release) { kernel_bdev->bd_ops->release(kernel_bdev); }

	/* Clear initialized flag */
	//e_blockdevice->flags &= ~EXT4_BDEV_INITIALIZED;

	return 0;
}


static inline struct block_device* __ext4_get_kernel_bdev(struct ext4_blockdev* e_blockdevice) {
	if (!e_blockdevice) return NULL;
	return (struct block_device*)e_blockdevice->bdif->p_user;
}