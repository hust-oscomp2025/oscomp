#include <kernel/fs/vfs.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/sched/sched.h>
#include <kernel/types.h>
#include <util/list.h>
#include <util/spinlock.h>
#include <util/string.h>

#include <spike_interface/spike_utils.h>

/* Head of the filesystem types list */
static struct list_head file_systems_list;
static spinlock_t file_systems_lock;

static struct superblock* __alloc_super(struct fsType* type);
static int lookup_dev_id(const char* dev_name, dev_t* dev_id);

/**
 * __alloc_super - Allocate a new superblock from a fsType
 * @type: Filesystem type
 *
 * Allocates and initializes a new superblock structure
 *
 * Returns a new superblock or NULL on failure
 */
static struct superblock* __alloc_super(struct fsType* type) {
	struct superblock* sb;

	sb = kmalloc(sizeof(struct superblock));
	if (!sb)
		return NULL;

	/* Initialize to zeros */
	memset(sb, 0, sizeof(struct superblock));

	/* Initialize lists */
	INIT_LIST_HEAD(&sb->s_list_all_inodes);
	spinlock_init(&sb->s_list_all_inodes_lock);

	INIT_LIST_HEAD(&sb->s_list_clean_inodes);
	INIT_LIST_HEAD(&sb->s_list_dirty_inodes);
	INIT_LIST_HEAD(&sb->s_list_io_inodes);
	spinlock_init(&sb->s_list_inode_states_lock);

	INIT_LIST_HEAD(&sb->s_list_mounts);

	INIT_LIST_HEAD(&sb->s_node_fsType);

	/* Initialize locks */
	spinlock_init(&sb->s_lock);
	spinlock_init(&sb->s_list_mounts_lock);

	/* Set up reference counts */
	sb->s_refcount = 1; /* Initial reference */
	//sb->s_active = 0;  /* No active references yet */

	/* Set filesystem type */
	sb->s_fsType = type;

	return sb;
}


/**
 * drop_super - Decrease reference count of superblock
 * @sb: Superblock to drop reference to
 *
 * Decrements the reference count and frees the superblock if
 * it reaches zero.
 */
void drop_super(struct superblock* sb) {
	if (!sb)
		return;
	if (atomic_dec_and_test(&sb->s_refcount)) {
		deactivate_super(sb);
	}
}

/**
 * deactivate_super - Clean up and free a superblock
 * @sb: Superblock to destroy
 *
 * Releases all resources associated with a superblock.
 * Should only be called when reference count reaches zero.
 */
static void deactivate_super(struct superblock* sb) {
	if (!sb)
		return;

	/* Call filesystem's put_super if defined */
	if (sb->s_operations && sb->s_operations->put_super)
		sb->s_operations->put_super(sb);

	spinlock_lock(&sb->s_fsType->fs_list_s_lock);
	/* Remove from filesystem's list */
	list_del(&sb->s_node_fsType);
	spinlock_unlock(&sb->s_fsType->fs_list_s_lock);

	/* Free any filesystem-specific info */
	if (sb->s_fs_specific)
		kfree(sb->s_fs_specific);

	/* Free the superblock itself */
	kfree(sb);
}

/**
 * grab_super - Increase active reference count
 * @sb: Superblock to reference
 *
 * Increases the active reference count of a superblock,
 * indicating that it's actively being used.
 */
void grab_super(struct superblock* sb) {
	if (!sb)
		return;

	spin_lock(&sb->s_lock);
	atomic_inc(&sb->s_refcount);
	//sb->s_active++;
	spin_unlock(&sb->s_lock);
}

/**
 * deactivate_super_safe - Decrease active reference count
 * @sb: Superblock to dereference
 *
 * Decreases the active reference count of a superblock.
 */
void deactivate_super_safe(struct superblock* sb) {
	if (!sb)
		return;

	spin_lock(&sb->s_lock);
	atomic_dec(&sb->s_refcount);

	//sb->s_active--;
	spin_unlock(&sb->s_lock);
}

/**
 * sync_filesystem - Synchronize a filesystem to disk
 * @sb: Superblock of filesystem to sync
 * @wait: Whether to wait for I/O completion
 *
 * Synchronizes all dirty inodes and other filesystem data to disk.
 *
 * Returns 0 on success, negative error code on failure
 */
int sync_filesystem(struct superblock* sb, int wait) {
	int ret = 0;
	struct inode *inode, *next;

	if (!sb)
		return -EINVAL;

	/* Call filesystem's sync_fs if defined */
	if (sb->s_operations && sb->s_operations->sync_fs) {
		ret = sb->s_operations->sync_fs(sb, wait);
		if (ret)
			return ret;
	}

	/* Sync all dirty inodes */
	spin_lock(&sb->s_list_inode_states_lock);
	list_for_each_entry_safe(inode, next, &sb->s_list_dirty_inodes, i_state_list_node) {
		if (sb->s_operations && sb->s_operations->write_inode) {
			spin_unlock(&sb->s_list_inode_states_lock);
			ret = sb->s_operations->write_inode(inode, wait);
			spin_lock(&sb->s_list_inode_states_lock);

			if (ret == 0 && wait) {
				/* Move from dirty list to LRU list */
				list_del(&inode->i_state_list_node);
				inode->i_state &= ~I_DIRTY;
				list_add(&inode->i_state_list_node, &sb->s_list_clean_inodes);
			}

			if (ret)
				break;
		}
	}
	spin_unlock(&sb->s_list_inode_states_lock);

	return ret;
}

/**
 * generic_shutdown_super - Generic superblock shutdown
 * @sb: Superblock to shut down
 *
 * Generic implementation for unmounting a filesystem.
 * Releases all inodes and drops the superblock.
 */
void generic_shutdown_super(struct superblock* sb) {
	struct inode *inode, *next;

	if (!sb)
		return;

	/* Write any dirty data */
	sync_filesystem(sb, 1);

	/* Free all inodes */
	spin_lock(&sb->s_list_all_inodes_lock);
	list_for_each_entry_safe(inode, next, &sb->s_list_all_inodes, i_s_list_node) {
		spin_unlock(&sb->s_list_all_inodes_lock);

		/* Forcibly evict the inode */
		if (inode->i_state & I_DIRTY) {
			if (sb->s_operations && sb->s_operations->write_inode)
				sb->s_operations->write_inode(inode, 1);
		}

		if (sb->s_operations && sb->s_operations->evict_inode)
			sb->s_operations->evict_inode(inode);
		else
			clear_inode(inode);

		spin_lock(&sb->s_list_all_inodes_lock);
	}
	spin_unlock(&sb->s_list_all_inodes_lock);

	/* Free root dentry */
	if (sb->s_global_root_dentry) {
		dput(sb->s_global_root_dentry);
		sb->s_global_root_dentry = NULL;
	}

	/* Decrease active count */
	deactivate_super_safe(sb);

	/* Drop reference */
	drop_super(sb);
}

/**
 * Register built-in filesystem types
 * called by vfs_init
 */
int fsType_register_all(void) {
	INIT_LIST_HEAD(&file_systems_list);
	spinlock_init(&file_systems_lock);
	int err;
	/* Register ramfs - our initial root filesystem */
	extern struct fsType ramfs_fsType;
	err = fsType_register(&ramfs_fsType);
	if (err < 0)
		return err;

	/* Register other built-in filesystems */
	extern struct fsType hostfs_fsType;
	err = fsType_register(&hostfs_fsType);
	if (err < 0)
		return err;

	return 0;
}

/**
 * fsType_register - Register a new filesystem type
 * @fs: The filesystem type structure to register
 *
 * Adds a filesystem to the kernel's list of filesystems that can be mounted.
 * Returns 0 on success, error code on failure.
 * fs是下层文件系统静态定义的，所以不需要分配内存
 */
int fsType_register(struct fsType* fs) {
	struct fsType* p;

	if (!fs || !fs->fs_name)
		return -EINVAL;

	/* Initialize filesystem type */
	INIT_LIST_HEAD(&fs->fs_node_gfslist);
	INIT_LIST_HEAD(&fs->fs_list_sb);

	/* Acquire lock for list manipulation */
	spinlock_init(&file_systems_lock);

	/* Check if filesystem already registered */
	list_for_each_entry(p, &file_systems_list, fs_node_gfslist) {
		if (strcmp(p->fs_name, fs->fs_name) == 0) {
			/* Already registered */
			release_spinlock(&file_systems_lock);
			sprint("VFS: Filesystem %s already registered\n", fs->fs_name);
			return -EBUSY;
		}
	}

	/* Add filesystem to the list (at the beginning for simplicity) */
	list_add(&fs->fs_node_gfslist, &file_systems_list);

	spinlock_unlock(&file_systems_lock);
	sprint("VFS: Registered filesystem %s\n", fs->fs_name);
	return 0;
}

/**
 * fsType_unregister - Remove a filesystem type from the kernel's list
 * @fs: The filesystem type structure to unregister
 *
 * Removes a filesystem from the kernel's list of available filesystems.
 * Returns 0 on success, error code on failure.
 */
int fsType_unregister(struct fsType* fs) {
	struct fsType* p;

	if (!fs || !fs->fs_name)
		return -EINVAL;

	/* Acquire lock for list manipulation */
	acquire_spinlock(&file_systems_lock);

	/* Find filesystem in the list */
	list_for_each_entry(p, &file_systems_list, fs_node_gfslist) {
		if (p == fs) {
			/* Found it - remove from the list */
			list_del(&p->fs_node_gfslist);
			release_spinlock(&file_systems_lock);
			sprint("VFS: Unregistered filesystem %s\n", p->fs_name);
			return 0;
		}
	}

	release_spinlock(&file_systems_lock);
	sprint("VFS: Filesystem %s not registered\n", fs->fs_name);
	return -ENOENT;
}

/**
 * fsType_lookup - Find a filesystem type by name
 * @name: The filesystem name to find
 *
 * Searches the list of registered filesystems for one with the given name.
 * Returns a pointer to the filesystem type structure or NULL if not found.
 */
struct fsType* fsType_lookup(const char* name) {
	struct fsType* fs;

	if (!name)
		return NULL;

	acquire_spinlock(&file_systems_lock);

	list_for_each_entry(fs, &file_systems_list, fs_node_gfslist) {
		if (strcmp(fs->fs_name, name) == 0) {
			release_spinlock(&file_systems_lock);
			return fs;
		}
	}

	release_spinlock(&file_systems_lock);
	return NULL;
}


/**
 * lookup_dev_id - Get device ID from device name
 * @dev_name: Name of the device
 * @dev_id: Output parameter for device ID
 *
 * Returns 0 on success, negative error code on failure
 */
static int lookup_dev_id(const char* dev_name, dev_t* dev_id) {
	/* Implementation would look up the device in the device registry */
	/* For now, we'll just use a simple hash function */
	if (!dev_name || !dev_id)
		return -EINVAL;

	*dev_id = 0;
	while (*dev_name) {
		*dev_id = (*dev_id * 31) + *dev_name++;
	}

	/* Ensure non-zero value for real devices */
	if (*dev_id == 0)
		*dev_id = 1;

	return 0;
}