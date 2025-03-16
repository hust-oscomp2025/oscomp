#include <errno.h>
#include <kernel/fs/vfs.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/sched/sched.h>
#include <util/string.h>

static void __clear_inode(struct inode* inode);

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
int generic_permission(struct inode* inode, int mask) {
  int mode = inode->i_mode;
  int res = 0;

  /* Root can do anything */
  if (current_task()->euid == 0)
    return 0;

  /* Nobody gets write access to a read-only filesystem */
  if ((mask & MAY_WRITE) && IS_RDONLY(inode))
    return -EROFS;

  /* Check if file is accessible by the user */
  if (current_task()->euid == inode->i_uid) {
    mode >>= 6; /* Use the user permissions */
  } else if (current_is_in_group(inode->i_gid)) {
    mode >>= 3; /* Use the group permissions */
  }

  /* Check if the mask is allowed in the mode */
  if ((mask & MAY_READ) && !(mode & S_IRUSR))
    res = -EACCES;
  if ((mask & MAY_WRITE) && !(mode & S_IWUSR))
    res = -EACCES;
  if ((mask & MAY_EXEC) && !(mode & S_IXUSR))
    res = -EACCES;

  return res;
}

/**
 * inode_permission - Check for access rights to a given inode
 * @inode: inode to check permission on
 * @mask: access mode to check for
 *
 * This is the main entry point for permission checking in the VFS.
 * It performs additional checks before delegating to filesystem-specific
 * permission methods.
 *
 * Returns 0 if access is allowed, negative error code otherwise.
 */
int inode_permission(struct inode* inode, int mask) {
  int retval;

  if (!inode)
    return -EINVAL;

  /* Always grant access to special inodes like device files */
  if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
    return 0;

  /* Use the fs-specific permission function if available */
  if (inode->i_op && inode->i_op->permission) {
    retval = inode->i_op->permission(inode, mask);
    if (retval != -EAGAIN)
      return retval;
  }

  /* Fall back to generic permission checking */
  return generic_permission(inode, mask);
}

/**
 * alloc_inode - Allocate a new inode for a specific filesystem
 * @sb: superblock the inode belongs to
 *
 * Allocates and initializes a new inode. If the superblock has
 * an alloc_inode operation, use that; otherwise allocate a generic inode.
 *
 * Returns the new inode or NULL if allocation failed.
 */
struct inode* alloc_inode(struct super_block* sb) {
  struct inode* inode;

  if (sb->s_op && sb->s_op->alloc_inode) {
    inode = sb->s_op->alloc_inode(sb);
  } else {
    inode = kmalloc(sizeof(struct inode));
  }

  if (!inode)
    return NULL;

  /* Initialize the inode */
  memset(inode, 0, sizeof(struct inode));
  atomic_set(&inode->i_count, 1);
  INIT_LIST_HEAD(&inode->i_dentry);
  spinlock_init(&inode->i_lock);
  inode->i_sb = sb;

  return inode;
}

/**
 * iget - Get inode from inode cache or create a new one
 * @sb: superblock to get inode from
 * @ino: inode number to look up
 *
 * Tries to find the specified inode in the inode cache. If not found,
 * allocates a new inode and calls the filesystem to read it.
 *
 * Returns the inode or NULL if an error occurs.
 */
struct inode* iget(struct super_block* sb, unsigned long ino) {
  struct inode* inode;

  /* Look in the inode hash table first */
  inode = iget_locked(sb, ino);
  if (!inode)
    return NULL;

  /* If the inode was already in the cache, just return it */
  if (!(inode->i_state & I_NEW))
    return inode;

  /* Initialize new inode from disk */
  if (sb->s_op && sb->s_op->read_inode) {
    sb->s_op->read_inode(inode);
  }

  /* Mark inode as initialized and wake up waiters */
  unlock_new_inode(inode);

  return inode;
}

/**
 * igrab - Increment inode reference count
 * @inode: inode to grab
 *
 * Increments the reference count on an inode if it's valid.
 *
 * Returns the inode with incremented count, or NULL if the inode is invalid.
 */
struct inode* igrab(struct inode* inode) {
  if (!inode || is_bad_inode(inode))
    return NULL;

  atomic_inc(&inode->i_count);
  return inode;
}

/**
 * put_inode - Release a reference to an inode
 * @inode: inode to put
 *
 * Decrements the reference count on an inode. If the count reaches zero,
 * the inode is deallocated or returned to the cache.
 */
void put_inode(struct inode* inode) {
  if (!inode)
    return;

  if (atomic_dec_and_test(&inode->i_count)) {
    if (inode->i_nlink == 0) {
      /* No links, evict the inode */
      evict_inode(inode);
    } else {
      /* Still has links, return to cache */
      /* Implement inode cache write-back here */
    }
  }
}

/**
 * is_bad_inode - Check if an inode is invalid
 * @inode: inode to check
 *
 * Returns true if the specified inode has been marked as bad.
 */
int is_bad_inode(struct inode* inode) {
  return (!inode || inode->i_op == NULL);
}

/**
 * setattr_prepare - Check if attribute change is allowed
 * @dentry: dentry of the inode to change
 * @attr: attributes to change
 *
 * Validates that the requested attribute changes are allowed
 * based on permissions and constraints.
 *
 * Returns 0 if the change is allowed, negative error code otherwise.
 */
int setattr_prepare(struct dentry* dentry, struct iattr* attr) {
  struct inode* inode = dentry->d_inode;
  int error = 0;

  if (!inode)
    return -EINVAL;

  /* Check for permission to change attributes */
  if (attr->ia_valid & ATTR_MODE) {
    error = inode_permission(inode, MAY_WRITE);
    if (error)
      return error;
  }

  /* Check if user can change ownership */
  if (attr->ia_valid & (ATTR_UID | ATTR_GID)) {
    /* Only root can change ownership */
    if (current_task()->euid != 0)
      return -EPERM;
  }

  /* Check if size can be changed */
  if (attr->ia_valid & ATTR_SIZE) {
    error = inode_permission(inode, MAY_WRITE);
    if (error)
      return error;

    /* Cannot change size of directories */
    if (S_ISDIR(inode->i_mode))
      return -EISDIR;
  }

  return 0;
}

/**
 * notify_change - Notify filesystem of attribute changes
 * @dentry: dentry of the changed inode
 * @attr: attributes that changed
 *
 * After validating attribute changes with setattr_prepare,
 * this function applies the changes and notifies the filesystem.
 *
 * Returns 0 on success, negative error code on failure.
 */
int notify_change(struct dentry* dentry, struct iattr* attr) {
  struct inode* inode = dentry->d_inode;
  int error;

  if (!inode)
    return -EINVAL;

  /* Validate changes */
  error = setattr_prepare(dentry, attr);
  if (error)
    return error;

  /* Call the filesystem's setattr method if available */
  if (inode->i_op && inode->i_op->setattr)
    return inode->i_op->setattr(dentry, attr);

  /* Apply attribute changes to the inode */
  if (attr->ia_valid & ATTR_MODE)
    inode->i_mode = attr->ia_mode;
  if (attr->ia_valid & ATTR_UID)
    inode->i_uid = attr->ia_uid;
  if (attr->ia_valid & ATTR_GID)
    inode->i_gid = attr->ia_gid;
  if (attr->ia_valid & ATTR_SIZE)
    inode->i_size = attr->ia_size;
  if (attr->ia_valid & ATTR_ATIME)
    inode->i_atime = attr->ia_atime;
  if (attr->ia_valid & ATTR_MTIME)
    inode->i_mtime = attr->ia_mtime;
  if (attr->ia_valid & ATTR_CTIME)
    inode->i_ctime = attr->ia_ctime;

  /* Mark the inode as dirty */
  mark_inode_dirty(inode);

  return 0;
}

/**
 * mark_inode_dirty - Mark an inode as needing writeback
 * @inode: inode to mark dirty
 *
 * Adds the inode to the superblock's list of dirty inodes
 * that need to be written back to disk.
 */
void mark_inode_dirty(struct inode* inode) {
  if (!inode)
    return;

  /* Add to superblock's dirty list if not already there */
  if (!(inode->i_state & I_DIRTY) && inode->i_sb) {
		
    spin_lock(&inode->i_sb->s_inode_list_lock);

    inode->i_state |= I_DIRTY;
    list_add(&inode->i_list, &inode->i_sb->s_dirty);
    spin_unlock(&inode->i_sb->s_inode_list_lock);
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
  if (!inode)
    return;

  /* Mark inode as being freed */
  inode->i_state |= I_FREEING;

  /* Call filesystem-specific cleanup through superblock operations */
  if (inode->i_sb && inode->i_sb->s_op && inode->i_sb->s_op->evict_inode) {
    inode->i_sb->s_op->evict_inode(inode);
  }

  /* Delete any remaining pages in the page cache */
  if (inode->i_mapping) {
    /* truncate_inode_pages(inode->i_mapping, 0); */
    /* Implementation would call truncate_inode_pages here */
  }

  /* Clean out the inode */
  __clear_inode(inode);
}

/**
 * __clear_inode - Clean up an inode and prepare it for freeing
 * @inode: inode to clear
 *
 * Final cleanup of an inode before it's memory is freed.
 */
void __clear_inode(struct inode* inode) {
  if (!inode)
    return;

  /* Remove any state flags */
  inode->i_state = 0;

  /* Remove from superblock lists */
  if (inode->i_sb) {
    spin_lock(&inode->i_sb->s_inode_list_lock);
    list_del_init(&inode->i_sb_list_node);
    list_del_init(&inode->i_state_list_node);
    spin_unlock(&inode->i_sb->s_inode_list_lock);
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