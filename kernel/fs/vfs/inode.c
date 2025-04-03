#include <kernel/mmu.h>
#include <kernel/sched.h>
#include <kernel/util/print.h>
#include <kernel/time.h>
#include <kernel/types.h>
#include <kernel/util.h>
#include <kernel/vfs.h>

void __inode__free(struct inode* inode);
void evict_inode(struct inode* inode);
int32 generic_permission(struct inode* inode, int32 mask);
void __unlock_new_inode(struct inode* inode);

/* Global inode hash table */
struct hashtable inode_hashtable;



/**
 * generic_permission - Check for access rights on a Unix-style file system
 * @inode: inode to check permissions on
 * @mask: access mode to check for (MAY_READ, MAY_WRITE, MAY_EXEC)
 *
 * Standard Unix permission checking implementation that most filesystems
 * can use directly or as a basis for their permission checking.
 *
 * Returns 0 if access is allowed, -EACCES otherwise.
 */
//  int32 generic_permission(struct inode* inode, int32 mask) {
// 	int32 mode = inode->i_mode;
// 	int32 res = 0;

// 	/* Root can do anything */
// 	if (current_task()->euid == 0) return 0;

// 	/* Nobody gets write access to a read-only filesystem */
// 	if ((mask & MAY_WRITE) && inode_isReadonly(inode)) return -EROFS;

// 	/* Check if file is accessible by the user */
// 	if (current_task()->euid == inode->i_uid) {
// 		mode >>= 6; /* Use the user permissions */
// 	} else if (current_is_in_group(inode->i_gid)) {
// 		mode >>= 3; /* Use the group permissions */
// 	}

// 	/* Check if the mask is allowed in the mode */
// 	if ((mask & MAY_READ) && !(mode & S_IRUSR)) res = -EACCES;
// 	if ((mask & MAY_WRITE) && !(mode & S_IWUSR)) res = -EACCES;
// 	if ((mask & MAY_EXEC) && !(mode & S_IXUSR)) res = -EACCES;

// 	return res;
// }

/**
 * inode_checkPermission - Check for access rights to a given inode
 * @inode: inode to check permission on
 * @mask: access mode to check for
 *
 * This is the main entry point for permission checking in the VFS.
 * It performs additional checks before delegating to filesystem-specific
 * permission methods.
 *
 * Returns 0 if access is allowed, negative error code otherwise.
 */
int32 inode_checkPermission(struct inode* inode, int32 mask) {
	int32 retval;

	if (!inode) return -EINVAL;

	/* Always grant access to special inodes like device files */
	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode)) return 0;

	/* Use the fs-specific permission function if available */
	if (inode->i_op && inode->i_op->permission) {
		retval = inode->i_op->permission(inode, mask);
		if (retval != -EAGAIN) return retval;
	}

	/* Fall back to generic permission checking */
	return generic_permission(inode, mask);
}

/**
 * inode_acquire - Get a fully initialized inode
 * @sb: superblock to get inode from
 * @ino: inode number to look up, 为0时表示分配一个新的inode
 *
 * Tries to find the specified inode in the inode cache. If not found,
 * allocates a new inode and calls the filesystem to read it.
 *
 * Returns the inode or NULL if an error occurs.
 */
struct inode* inode_acquire(struct superblock* sb, uint64 ino) {
	CHECK_PTR_VALID(sb, ERR_PTR(-EINVAL));

	struct inode* inode = icache_lookup(sb, ino);
	CHECK_PTR_ERROR(inode, ERR_PTR(-ENOMEM));

	if (!inode) {
		inode = superblock_createInode(sb);
		CHECK_PTR_VALID(inode, ERR_PTR(-ENOMEM));
	}

	if (inode->i_state & I_NEW) {
		// I_NEW 标记由文件系统设置，表示 inode 尚未完成 I/O 初始化
		if (sb->s_operations && sb->s_operations->read_inode) {
			sb->s_operations->read_inode(inode);
			// inode 初始化完成后的解锁动作由具体文件系统负责
		} else {
			sprint("No read_inode method for superblock\n");
			return ERR_PTR(-EINVAL);
		}
	}
	icache_insert(inode);

	// 原则上栈上使用的inode指针必须解引用，但是返回时又需要增加引用，所以在return前不做动作
	return inode;
}

/**
 * inode_ref - Increment inode reference count
 * @inode: inode to grab
 *
 * Increments the reference count on an inode if it's valid.
 *
 * Returns the inode with incremented count, or NULL if the inode is invalid.
 */
struct inode* inode_ref(struct inode* inode) {
	if (!inode || inode_isBad(inode)) return NULL;

	atomic_inc(&inode->i_refcount);
	return inode;
}

/**
 * inode_unref - Release a reference to an inode
 * @inode: inode to put
 *
 * Decrements the reference count on an inode. If the count reaches zero,
 * the inode is added to the LRU list waiting for recycle.
 */
void inode_unref(struct inode* inode) {
	if (unlikely(!inode)) return;

	struct superblock* sb = inode->i_superblock;
	spinlock_lock(&inode->i_lock);
	/* Decrease reference count */
	if (atomic_dec_and_test(&inode->i_refcount)) {
		/* Last reference gone - add to superblock's LRU */
		if (!(inode->i_state & I_DIRTY)) {
			/* If it's on another state list, remove it first */
			spinlock_lock(&sb->s_list_inode_states_lock);
			if (!list_empty(&inode->i_state_list_node)) {
				list_del_init(&inode->i_state_list_node);
			}

			list_add_tail(&inode->i_state_list_node, &sb->s_list_clean_inodes);
			spinlock_unlock(&sb->s_list_inode_states_lock);
		}
	}
	spinlock_unlock(&inode->i_lock);
}

/**
 * inode_setDirty - Mark an inode as needing writeback
 * @inode: inode to mark dirty
 *
 * Adds the inode to the superblock's list of dirty inodes
 * that need to be written back to disk.
 */
void inode_setDirty(struct inode* inode) {
	if (!inode) return;

	struct superblock* sb = inode->i_superblock;

	/* Add to superblock's dirty list if not already there */
	if (!(inode->i_state & I_DIRTY) && sb) {
		spinlock_lock(&sb->s_list_inode_states_lock);
		spinlock_lock(&inode->i_lock);

		if (likely(!list_empty(&inode->i_state_list_node))) {
			list_del_init(&inode->i_state_list_node);
		}

		inode->i_state |= I_DIRTY;
		list_add(&inode->i_state_list_node, &sb->s_list_dirty_inodes);

		spinlock_unlock(&inode->i_lock);
		spinlock_unlock(&sb->s_list_inode_states_lock);
	}
}

/**
 * evict_inode - Remove an inode from the filesystem
 * @inode: inode to evict
 *
 * This function is called when an inode should be removed from
 * the filesystem (when its link count reaches zero). It handles
 * cleaning up filesystem-specific resources and then clearing
 * the inode.
 */
void evict_inode(struct inode* inode) {
	if (!inode) return;

	/* Mark inode as being freed */
	inode->i_state |= I_FREEING;

	/* Call filesystem-specific cleanup through superblock operations */
	if (inode->i_superblock && inode->i_superblock->s_operations && inode->i_superblock->s_operations->evict_inode) {
		inode->i_superblock->s_operations->evict_inode(inode);
	}

	/* Delete any remaining pages in the page cache */
	if (inode->i_mapping) {
		/* truncate_inode_pages(inode->i_mapping, 0); */
		/* Implementation would call truncate_inode_pages here */
	}

	/* Clean out the inode */
	__inode__free(inode);
}

/**
 * __inode__free - Clean up an inode and prepare it for freeing
 * @inode: inode to clear
 *
 * Final cleanup of an inode before it's memory is freed.
 */
void __inode__free(struct inode* inode) {
	if (!inode) return;

	/* Remove any state flags */
	inode->i_state = 0;

	/* Remove from superblock lists */
	if (inode->i_superblock) {
		spinlock_lock(&inode->i_superblock->s_list_all_inodes_lock);
		list_del_init(&inode->i_s_list_node);
		list_del_init(&inode->i_state_list_node);
		spinlock_unlock(&inode->i_superblock->s_list_all_inodes_lock);
	}

	/* Clear file system specific data if needed */
	if (inode->i_fs_info) {
		kfree(inode->i_fs_info);
		inode->i_fs_info = NULL;
	}

	/* Clear block mapping data if present */
	if (inode->i_data) {
		kfree(inode->i_data);
		inode->i_data = NULL;
	}

	/* Free the inode memory */
	kfree(inode);
}

/**
 * Compare two inode keys for equality
 */
int32 icache_equal(const void* k1, const void* k2) {
	const struct inode_key *key1 = k1, *key2 = k2;

	return (key1->sb == key2->sb && key1->ino == key2->ino);
}



/**
 * __unlock_new_inode - Unlock a newly allocated inode
 * @inode: inode to unlock
 *
 * Clears the I_NEW state bit and wakes up any processes
 * waiting on this inode to be initialized.
 */
int unlock_new_inode(struct inode* inode) {
	if (!inode) return -EINVAL;
	if (!(inode->i_state & I_NEW)) {
		// this should never happen
		return -EINVAL;
	}

	spinlock_lock(&inode->i_lock);
	inode->i_state &= ~I_NEW;
	spinlock_unlock(&inode->i_lock);

	/* Wake up anyone waiting for this inode to be initialized */
	wake_up_inode(inode);
	return 0;
}

/**
 * inode_sync - Write an inode to disk
 * @inode: inode to write
 * @wait: whether to wait for I/O to complete
 *
 * Calls the filesystem's writeback function to synchronize
 * the inode to disk.
 *
 * Returns 0 on success or negative error code.
 */
int32 inode_sync(struct inode* inode, int32 wait) {
	int32 ret = 0;

	if (!inode) return -EINVAL;

	/* If clean, nothing to do */
	if (!(inode->i_state & I_DIRTY)) return 0;

	/* Call the filesystem's write_inode method if available */
	if (inode->i_superblock && inode->i_superblock->s_operations && inode->i_superblock->s_operations->write_inode) {
		ret = inode->i_superblock->s_operations->write_inode(inode, wait);
	}

	/* If successful and waiting requested, clear dirty state */
	if (ret == 0 && wait) {
		spinlock_lock(&inode->i_superblock->s_list_inode_states_lock);
		/* Remove from dirty list */
		if (inode->i_state & I_DIRTY) {
			list_del_init(&inode->i_state_list_node);
			inode->i_state &= ~I_DIRTY;
			list_add(&inode->i_state_list_node, &inode->i_superblock->s_list_clean_inodes);
		}
		spinlock_unlock(&inode->i_superblock->s_list_inode_states_lock);
	}

	return ret;
}

/**
 * inode_sync_metadata - Sync only inode metadata to disk
 * @inode: inode to sync
 * @wait: whether to wait for I/O to complete
 *
 * Like inode_sync(), but only writes inode metadata, not data blocks.
 * Used when only attributes have changed.
 *
 * Returns 0 on success or negative error code.
 */
int32 inode_sync_metadata(struct inode* inode, int32 wait) {
	/* For now, just delegate to inode_sync */
	/* In a more complete implementation, this would only sync metadata */
	return inode_sync(inode, wait);
}

/**
 * wake_up_inode - Wake up processes waiting on an inode
 * @inode: the inode to wake up waiters for
 *
 * Helper function to wake up processes waiting for an inode
 * to become available (e.g., after initialization).
 */
void wake_up_inode(struct inode* inode) {
	/* In a full implementation, this would use wait queues */
	/* For now, we'll assume no waiters or that it's handled elsewhere */
}

/**
 * Get key from an inode hash node
 * This function extracts the key (superblock + inode number) from a list node
 */
void* icache_getkey(struct list_node* node) {
	struct inode* inode = container_of(node, struct inode, i_hash_node);
	return &inode->i_key;
}

/**
 * inode_mkdir - Core VFS directory creation implementation
 * @dir: Parent directory inode
 * @dentry: Dentry for the new directory
 * @mode: Directory permissions
 *
 * 这个函数中的dentry，及它在dentry目录树上的关系，都是预先创建好的
 * 它只负责在目录inode中把dentry加进去
 * Creates a new directory with VFS defaults, calling filesystem-specific
 * extension points when available.
 *
 * Returns: 0 on success, negative error code on failure
 */
int32 inode_mkdir(struct inode* dir, struct dentry* dentry, mode_t mode) {

	int32 error = 0;

	if (!dir || !dentry) return -EINVAL;

	/* Check if directory is writable */
	error = inode_permission(dir, MAY_WRITE | MAY_EXEC);
	if (error) return error;

	/* Call filesystem-specific mkdir if available */
	if (dir->i_op && dir->i_op->mkdir) {
		return dir->i_op->mkdir(dir, dentry, mode);
	}

	/* Default implementation if no filesystem handler */
	struct inode* inode = inode_acquire(dir->i_superblock, 0);
	if (!inode) return -ENOMEM;

	/* Set up basic directory attributes */
	inode->i_mode = S_IFDIR | (mode & 0777);
	inode->i_uid = current_task()->uid;
	inode->i_gid = current_task()->gid;
	inode->i_size = 0;
	inode->i_blocks = 0;
	inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(dir->i_superblock);

	/* Set up directory operations */
	// inode->i_op = &simple_dir_inode_operations;
	// inode->i_fop = &simple_dir_operations;

	/* Link inode to dentry */
	dentry_instantiate(dentry, inode);

	/* Update parent directory */
	dir->i_mtime = dir->i_ctime = current_time(dir->i_superblock);
	dir->i_size++;

	return 0;
}

/**
 * inode_mknod - Core VFS special file creation implementation
 * @dir: Parent directory inode
 * @dentry: Dentry for the new device node
 * @mode: File mode including type (S_IFBLK, S_IFCHR, etc.)
 * @dev: Device number for device nodes
 *
 * Creates a new special file (device node, fifo, socket) with VFS defaults.
 * Filesystem-specific mknod handlers are called if available, otherwise
 * uses generic implementation suitable for in-memory filesystems like ramfs.
 *
 * Returns: 0 on success, negative error code on failure
 */
int32 inode_mknod(struct inode* dir, struct dentry* dentry, mode_t mode, dev_t dev) {
	int32 error = 0;

	if (!dir || !dentry) return -EINVAL;

	/* Check if directory is writable */
	error = inode_permission(dir, MAY_WRITE | MAY_EXEC);
	if (error) return error;

	/* Call filesystem-specific mknod if available */
	if (dir->i_op && dir->i_op->mknod) {
		return dir->i_op->mknod(dir, dentry, mode, dev);
	}

	/* Default implementation for simple filesystems like ramfs */
	struct inode* inode = inode_acquire(dir->i_superblock, 0);
	if (PTR_IS_ERROR(inode)) return PTR_ERR(inode);
	// TODO: 做一个通用的inode_init方法。
	// /* Set up basic inode attributes */
	// inode->i_mode = mode;
	// inode->i_uid = current_task()->uid;
	// inode->i_gid = current_task()->gid;
	// inode->i_size = 0;
	// inode->i_blocks = 0;
	// inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(dir->i_superblock);
	// extern const struct file_operations bdFile_operations, chFile_operations, fifoFile_operations, socketFile_operations;

	// /* Handle different node types */
	// if (S_ISBLK(mode) || S_ISCHR(mode)) {
	// 	inode->i_rdev = dev; /* Store device ID in inode */

	// 	if (S_ISBLK(mode)) {
	// 		/* Block device operations */
	// 		inode->i_fop = &bdFile_operations;
	// 	} else {
	// 		/* Character device operations */
	// 		inode->i_fop = &chFile_operations;
	// 	}
	// } else if (S_ISFIFO(mode)) {
	// 	/* FIFO (named pipe) operations */
	// 	inode->i_fop = &fifoFile_operations;
	// } else if (S_ISSOCK(mode)) {
	// 	/* Unix socket operations */
	// 	inode->i_fop = &socketFile_operations;
	// }

	/* Link inode to dentry */
	dentry_instantiate(dentry, inode);

	/* Update parent directory */
	dir->i_mtime = dir->i_ctime = current_time(dir->i_superblock);
	dir->i_size++;

	return 0;
}

/**
 * inode_permission - Check for access rights to a given inode
 * @inode: inode to check permission on
 * @mask: access mode to check for (MAY_READ, MAY_WRITE, MAY_EXEC, etc.)
 *
 * Main entry point for permission checking in the VFS.
 * Performs standard Unix-style permission checking, with filesystem-specific
 * extensions possible but not required.
 *
 * Returns 0 if access is allowed, negative error code otherwise.
 */
int32 inode_permission(struct inode* inode, int32 mask) {
	int32 mode;
	int32 retval = 0;

	if (!inode) return -EINVAL;

	/* Root can do (almost) anything */
	if (current_task()->euid == 0) {
		/* Even root has some restrictions */
		if ((mask & MAY_WRITE) && inode_isImmutable(inode)) return -EPERM;
		return 0;
	}

	/* Call filesystem-specific permission if available */
	if (inode->i_op && inode->i_op->permission) {
		retval = inode->i_op->permission(inode, mask);
		if (retval != -EAGAIN) /* Filesystem handled it directly */
			return retval;
	}

	/* Default permission implementation */
	mode = inode->i_mode;

	/* Nobody gets write access to a read-only filesystem */
	if ((mask & MAY_WRITE) && inode_isReadonly(inode)) return -EROFS;

	/* Check standard Unix permissions */
	if (current_task()->euid == inode->i_uid) {
		/* Owner permissions */
		mode >>= 6;
	} else if (current_group_matches(inode->i_gid)) {
		/* Group permissions */
		mode >>= 3;
	}
	/* Everyone else gets the "other" bits */

	/* Check if the mask is allowed in the mode */
	if ((mask & MAY_READ) && !(mode & 4)) /* 4 = read bit */
		return -EACCES;
	if ((mask & MAY_WRITE) && !(mode & 2)) /* 2 = write bit */
		return -EACCES;
	if ((mask & MAY_EXEC) && !(mode & 1)) /* 1 = execute bit */
		return -EACCES;

	return 0;
}

/**
 * inode_isReadonly - Check if inode is on a read-only filesystem
 */
bool inode_isReadonly(struct inode* inode) {
	/* Check if the superblock is mounted read-only */
	return (inode->i_superblock && (inode->i_superblock->s_flags & MS_RDONLY));
}

/**
 * inode_isImmutable - Check if inode is immutable
 */
bool inode_isImmutable(struct inode* inode) {
	/* Immutable flag would typically be in inode attributes or flags */
	/* For simplicity, just returning false for now */
	return 0;
}

/**
 * inode_rmdir - Remove a directory
 * @dir: Parent directory's inode
 * @dentry: Directory to remove
 *
 * Returns 0 on success or negative error code
 */
int32 inode_rmdir(struct inode* dir, struct dentry* dentry) {
	int32 error;

	if (!dir || !dentry || !dentry->d_inode) return -EINVAL;

	if (!dir->i_op || !dir->i_op->rmdir) return -EPERM;

	/* Check directory permissions */
	error = inode_checkPermission(dir, MAY_WRITE | MAY_EXEC);
	if (error) return error;

	/* Cannot remove non-empty directory */
	if (!dentry_isEmptyDir(dentry)) return -ENOTEMPTY;

	return dir->i_op->rmdir(dir, dentry);
}

/**
 * generic_permission - Check for access rights on a Unix-style file system
 * @inode: inode to check permissions on
 * @mask: access mode to check for (MAY_READ, MAY_WRITE, MAY_EXEC)
 *
 * Standard Unix permission checking implementation that most filesystems
 * can use directly or as a basis for their permission checking.
 *
 * Returns 0 if access is allowed, -EACCES otherwise.
 */
int32 generic_permission(struct inode* inode, int32 mask) {
	int32 mode = inode->i_mode;
	int32 res = 0;

	/* Root can do anything */
	if (current_task()->euid == 0) return 0;

	/* Nobody gets write access to a read-only filesystem */
	if ((mask & MAY_WRITE) && inode_isReadonly(inode)) return -EROFS;

	/* Check if file is accessible by the user */
	if (current_task()->euid == inode->i_uid) {
		mode >>= 6; /* Use the user permissions */
	} else if (current_group_matches(inode->i_gid)) {
		mode >>= 3; /* Use the group permissions */
	}
	/* Otherwise we're already at "other" permissions */

	/* Check if the mask is allowed in the mode */
	if ((mask & MAY_READ) && !(mode & 4)) /* 4 = read bit */
		res = -EACCES;
	if ((mask & MAY_WRITE) && !(mode & 2)) /* 2 = write bit */
		res = -EACCES;
	if ((mask & MAY_EXEC) && !(mode & 1)) /* 1 = execute bit */
		res = -EACCES;

	return res;
}

bool inode_isDir(struct inode* inode) {
	if (!inode) return false;
	return S_ISDIR(inode->i_mode);
}

struct dentry* inode_lookup(struct inode* dir, struct dentry* dentry, uint32 lookup_flags) {
	if (!dir || !dentry) return ERR_PTR(-EINVAL);
	if (!inode_isDir(dir)) return ERR_PTR(-ENOTDIR);
	if (dir->i_op && dir->i_op->lookup) {
		return dir->i_op->lookup(dir, dentry, lookup_flags);
	}
	return ERR_PTR(-ENOSYS);
}