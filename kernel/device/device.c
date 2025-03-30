#include <kernel/device/block_device.h>
#include <kernel/sprint.h>
#include <kernel/util.h>
#include <kernel/vfs.h>

static void __register_platform_devices(void);

// declared in block_device.c
extern struct list_head block_device_list;
extern spinlock_t block_device_list_lock;

/* RAM disk operations (simplified pseudo-code) */
static struct block_operations ramdisk_ops = {
    // .open = ramdisk_open,
    // .release = ramdisk_release,
    // .read_blocks = ramdisk_read_blocks,
    // .write_blocks = ramdisk_write_block,
    // .ioctl = ramdisk_ioctl,
};

/* Memory device operations (simplified pseudo-code) */
static struct block_operations mem_blk_ops = {
    // .open = mem_open,
    // .release = mem_release,
    // .read_blocks = mem_read_blocks,
    // .write_blocks = mem_write_block,
};

/**
 * Create device nodes in the /dev directory
 */
int32 create_device_nodes(void) {
	struct dentry* dev_dir;
	struct block_device* bdev;
	char name[32];
	int32 ret;
	mode_t mode = S_IFBLK | 0600;

	/* Create /dev directory if it doesn't exist */
	dev_dir = vfs_mkdir(NULL, "/dev", 0755);
	if (PTR_IS_ERROR(dev_dir)) {
		sprint("Failed to create /dev directory: %d\n", PTR_ERR(dev_dir));
		return PTR_ERR(dev_dir);
	}

	/* Lock the block device list */
	spinlock_lock(&block_device_list_lock);

	/* Iterate through registered block devices and create nodes */
	list_for_each_entry(bdev, &block_device_list, bd_list) {
		/* Format device name based on device type */
		if (MAJOR(bdev->bd_dev) == RAMDISK_MAJOR)
			snprintf(name, 32, "ram%d", MINOR(bdev->bd_dev));
		else if (MAJOR(bdev->bd_dev) == SCSI_DISK0_MAJOR)
			snprintf(name, 32, "sd%c%d", 'a' + (MINOR(bdev->bd_dev) / 16), MINOR(bdev->bd_dev) % 16);
		else
			snprintf(name, 32, "blk%d_%d", MAJOR(bdev->bd_dev), MINOR(bdev->bd_dev));

		/* Create the device node */
		struct dentry* nod_dentry = vfs_mknod(dev_dir, name, mode, bdev->bd_dev);
		if (ERR_PTR(nod_dentry)) {
			sprint("Failed to create device node /dev/%s: %d\n", name, ret);
			/* Continue with other devices */
		} else {
			sprint("Created device node /dev/%s (dev=0x%x)\n", name, bdev->bd_dev);
		}
		dentry_unref(nod_dentry);
	}

	spinlock_unlock(&block_device_list_lock);
	return 0;
}

/**
 * Initialize device system during boot
 */
void device_init(void) {
	INIT_LIST_HEAD(&block_device_list);
	/* Initialize block device subsystem */
	block_dev_init();

	/* Register platform-specific devices */
	__register_platform_devices();

	/* Create device nodes in /dev */
	create_device_nodes();

	sprint("Device initialization complete\n");
}

/**
 * __register_platform_devices - Register platform-specific block devices
 *
 * This function registers built-in devices like RAM disk, NVME, etc.
 */
static void __register_platform_devices(void) {
	/* Register RAM disk device */
	struct block_device* ram_dev = alloc_block_device();
	if (ram_dev) {
		ram_dev->bd_dev = MKDEV(RAMDISK_MAJOR, 0);
		ram_dev->bd_block_size = 512;
		ram_dev->bd_ops = &ramdisk_ops; /* You'll need to define this */

		/* Add to global list */
		spinlock_lock(&block_device_list_lock);
		list_add(&ram_dev->bd_list, &block_device_list);
		spinlock_unlock(&block_device_list_lock);

		sprint("Registered RAM disk device (major=%d, minor=%d)\n", MAJOR(ram_dev->bd_dev), MINOR(ram_dev->bd_dev));
	}

	/* Register virtual disk for disk images (if applicable) */
	/* Register NVMe devices (platform specific) */
	/* Register SCSI/SATA devices (platform specific) */

	/*
	 * In a real system, you would scan PCI bus, SATA controllers, etc.
	 * For now, we'll just use a RAM disk for simplicity
	 */
}

/**
 * lookup_dev_id - Get device ID from device name
 * @dev_name: Name of the device (path in the VFS)
 * @dev_id: Output parameter for device ID
 *
 * Looks up a device by its path (like "/dev/sda1") and returns its
 * device ID by querying the VFS.
 *
 * Returns 0 on success, negative error code on failure
 */
int32 lookup_dev_id(const char* dev_name, dev_t* dev_id) {
	struct path path;
	struct inode* inode;
	int32 ret;

	if (!dev_name || !dev_id) return -EINVAL;

	/* Empty device name is invalid */
	if (!*dev_name) return -ENODEV;

	/* Special case for memory filesystems */
	if (strcmp(dev_name, "none") == 0 || strcmp(dev_name, "mem") == 0 || strcmp(dev_name, "memory") == 0) {
		*dev_id = 0;
		return 0;
	}

	/* Look up the path in the VFS */
	ret = path_create(dev_name, LOOKUP_FOLLOW, &path);
	if (ret < 0) {
		sprint("VFS: Cannot find device path %s, error=%d\n", dev_name, ret);
		return ret;
	}

	/* Get the inode from the found dentry */
	inode = path.dentry->d_inode;
	if (!inode) {
		path_destroy(&path);
		sprint("VFS: No inode for %s\n", dev_name);
		return -ENODEV;
	}

	/* Make sure it's a block device */
	if (!S_ISBLK(inode->i_mode)) {
		path_destroy(&path);
		sprint("VFS: %s is not a block device (mode=%x)\n", dev_name, inode->i_mode);
		return -ENOTBLK;
	}

	/* Get the device ID from the inode */
	*dev_id = inode->i_rdev;
	sprint("VFS: Found device %s with ID %lx\n", dev_name, *dev_id);

	/* Clean up */
	path_destroy(&path);
	return 0;
}
