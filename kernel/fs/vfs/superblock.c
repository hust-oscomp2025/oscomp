#include <kernel/fs/vfs/vfs.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/sched/sched.h>
#include <kernel/types.h>
#include <util/list.h>
#include <util/spinlock.h>
#include <util/string.h>

#include <spike_interface/spike_utils.h>





/**
 * superblock_put - Decrease reference count of superblock
 * @sb: Superblock to drop reference to
 *
 * Decrements the reference count and frees the superblock if
 * it reaches zero.
 */
void superblock_put(struct superblock* sb) {
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

	spinlock_lock(&sb->s_fstype->fs_list_superblock_lock);
	/* Remove from filesystem's list */
	list_del(&sb->s_node_fstype);
	spinlock_unlock(&sb->s_fstype->fs_list_superblock_lock);

	/* Free any filesystem-specific info */
	if (sb->s_fs_info)
		kfree(sb->s_fs_info);

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

	spinlock_lock(&sb->s_lock);
	atomic_inc(&sb->s_refcount);
	//sb->s_active++;
	spinlock_unlock(&sb->s_lock);
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

	spinlock_lock(&sb->s_lock);
	atomic_dec(&sb->s_refcount);

	//sb->s_active--;
	spinlock_unlock(&sb->s_lock);
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
	spinlock_lock(&sb->s_list_inode_states_lock);
	list_for_each_entry_safe(inode, next, &sb->s_list_dirty_inodes, i_state_list_node) {
		if (sb->s_operations && sb->s_operations->write_inode) {
			spinlock_unlock(&sb->s_list_inode_states_lock);
			ret = sb->s_operations->write_inode(inode, wait);
			spinlock_lock(&sb->s_list_inode_states_lock);

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
	spinlock_unlock(&sb->s_list_inode_states_lock);

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
	spinlock_lock(&sb->s_list_all_inodes_lock);
	list_for_each_entry_safe(inode, next, &sb->s_list_all_inodes, i_s_list_node) {
		spinlock_unlock(&sb->s_list_all_inodes_lock);

		/* Forcibly evict the inode */
		if (inode->i_state & I_DIRTY) {
			if (sb->s_operations && sb->s_operations->write_inode)
				sb->s_operations->write_inode(inode, 1);
		}

		if (sb->s_operations && sb->s_operations->evict_inode)
			sb->s_operations->evict_inode(inode);
		else
			clear_inode(inode);

		spinlock_lock(&sb->s_list_all_inodes_lock);
	}
	spinlock_unlock(&sb->s_list_all_inodes_lock);

	/* Free root dentry */
	if (sb->s_root) {
		dput(sb->s_root);
		sb->s_root = NULL;
	}

	/* Decrease active count */
	deactivate_super_safe(sb);

	/* Drop reference */
	superblock_put(sb);
}



/**
 * superblock_acquireMount - Create a mount point for a superblock
 * @sb: Superblock to create mount for
 * @flags: Mount flags
 * @device_path: Path to the device being mounted (may be NULL)
 *
 * Creates a new mount structure for the given superblock,
 * either using filesystem-specific mount creation if available,
 * or the generic mount creation method.
 *
 * The returned mount has its reference count set to 1.
 *
 * Returns: A new mount structure on success, NULL on failure
 */
struct vfsmount* superblock_acquireMount(struct superblock* sb, int flags, const char* device_path) {
    struct vfsmount* mnt = NULL;
    static int mount_id = 0;
    
    if (!sb || !sb->s_root)
        return NULL;
        
    // First try filesystem-specific mount creation if available
    if (sb->s_operations && sb->s_operations->create_mount) {
        mnt = sb->s_operations->create_mount(sb, flags, device_path, NULL);
        if (mnt)
            return mnt;
    }
    
    // Fall back to generic mount creation
    mnt = kmalloc(sizeof(struct vfsmount));
	CHECK_PTR(mnt, ERR_PTR(-ENOMEM));

    // Initialize the mount structure
    memset(mnt, 0, sizeof(struct vfsmount));
    atomic_set(&mnt->mnt_refcount, 1);
    mnt->mnt_superblock = sb;
    mnt->mnt_root = dentry_get(sb->s_root);
    mnt->mnt_flags = flags;
    mnt->mnt_id = mount_id++;
    
    // Store device name if provided
    if (device_path && *device_path) {
        mnt->mnt_devname = kstrdup(device_path, GFP_KERNEL);
    }
    
    // Initialize list heads
    INIT_LIST_HEAD(&mnt->mnt_list_children);
    
    // Initialize list nodes
    INIT_LIST_HEAD(&mnt->mnt_node_superblock);
    INIT_LIST_HEAD(&mnt->mnt_node_parent);
    INIT_LIST_HEAD(&mnt->mnt_node_global);
    INIT_LIST_HEAD(&mnt->mnt_node_namespace);
    
    // Add to the superblock's mount list
    spinlock_lock(&sb->s_list_mounts_lock);
    list_add(&mnt->mnt_node_superblock, &sb->s_list_mounts);
    spinlock_unlock(&sb->s_list_mounts_lock);
    
    // Increment superblock reference count
    grab_super(sb);
    
    return mnt;
}



