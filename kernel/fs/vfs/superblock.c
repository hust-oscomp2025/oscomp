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

static struct super_block* __alloc_super(struct file_system_type* type);
static int lookup_dev_id(const char* dev_name, dev_t* dev_id);

/**
 * __alloc_super - Allocate a new superblock from a fstype
 * @type: Filesystem type
 *
 * Allocates and initializes a new superblock structure
 *
 * Returns a new superblock or NULL on failure
 */
static struct super_block* __alloc_super(struct file_system_type* type) {
  struct super_block* sb;

  sb = kmalloc(sizeof(struct super_block));
  if (!sb)
    return NULL;

  /* Initialize to zeros */
  memset(sb, 0, sizeof(struct super_block));

  /* Initialize lists */
  INIT_LIST_HEAD(&sb->all_inodes);
  spinlock_init(&sb->all_inodes_lock);

  INIT_LIST_HEAD(&sb->clean_inodes);
  INIT_LIST_HEAD(&sb->dirty_inodes);
  INIT_LIST_HEAD(&sb->io_inodes);
  spinlock_init(&sb->inode_states_lock);

  INIT_LIST_HEAD(&sb->s_mounts_list);

  INIT_LIST_HEAD(&sb->fstype_node);

  /* Initialize locks */
  spinlock_init(&sb->superblock_lock);
  spinlock_init(&sb->s_mounts_lock);

  /* Set up reference counts */
  sb->ref_count = 1;  /* Initial reference */
  sb->s_active = 0; /* No active references yet */

  /* Set filesystem type */
  sb->fs_type = type;

  return sb;
}

/**
 * get_superblock - Get or create a superblock
 * @type: Filesystem type
 * @dev_id: Device identifier (0 for virtual filesystems)
 * @fs_data: Filesystem-specific mount data
 *
 * Returns an existing superblock for the device or creates a new one.
 * Increments the reference count on the returned superblock.
 *
 * Returns: pointer to superblock or NULL on failure
 */
struct super_block* get_superblock(struct file_system_type* type, dev_t dev_id,
                                   void* fs_data) {
  struct super_block* sb = NULL;

  if (!type)
    return NULL;

  /* Lock to protect the filesystem type's superblock list */
  spin_lock(&type->superblock_list_lock);

  /* Check if a superblock already exists for this device */
  if (dev_id != 0) {
    list_for_each_entry(sb, &type->superblock_list, fstype_node) {
      if (sb->device_id == dev_id) {
        /* Found matching superblock - increment reference */
        sb->ref_count++;
        spin_unlock(&type->superblock_list_lock);
        return sb;
      }
    }
  }

  /* No existing superblock found, allocate a new one */
  spin_unlock(&type->superblock_list_lock);
  sb = __alloc_super(type);
  if (!sb)
    return NULL;

  /* Set device ID */
  sb->device_id = dev_id;

  /* Store filesystem-specific data if provided */
  if (fs_data) {
    /* Note: Filesystem is responsible for managing this data */
    sb->fs_specific_data = fs_data;
  }

  /* Add to the filesystem's list of superblocks */
  spin_lock(&type->superblock_list_lock);
  list_add(&sb->fstype_node, &type->superblock_list);
  spin_unlock(&type->superblock_list_lock);

  return sb;
}
/**
 * drop_super - Decrease reference count of superblock
 * @sb: Superblock to drop reference to
 *
 * Decrements the reference count and frees the superblock if
 * it reaches zero.
 */
void drop_super(struct super_block* sb) {
  if (!sb)
    return;
	if(atomic_dec_and_test(&sb->ref_count)){
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
static void deactivate_super(struct super_block* sb) {
  if (!sb)
    return;

  /* Call filesystem's put_super if defined */
  if (sb->operations && sb->operations->put_super)
    sb->operations->put_super(sb);

	spinlock_lock(&sb->fs_type->superblock_list_lock);
  /* Remove from filesystem's list */
  list_del(&sb->fstype_node);
	spinlock_unlock(&sb->fs_type->superblock_list_lock);

  /* Free any filesystem-specific info */
  if (sb->fs_specific_data)
    kfree(sb->fs_specific_data);

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
void grab_super(struct super_block* sb) {
  if (!sb)
    return;

  spin_lock(&sb->superblock_lock);
  sb->s_active++;
  spin_unlock(&sb->superblock_lock);
}

/**
 * deactivate_super_safe - Decrease active reference count
 * @sb: Superblock to dereference
 *
 * Decreases the active reference count of a superblock.
 */
void deactivate_super_safe(struct super_block* sb) {
  if (!sb)
    return;

  spin_lock(&sb->superblock_lock);
  sb->s_active--;
  spin_unlock(&sb->superblock_lock);
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
int sync_filesystem(struct super_block* sb, int wait) {
  int ret = 0;
  struct inode *inode, *next;

  if (!sb)
    return -EINVAL;

  /* Call filesystem's sync_fs if defined */
  if (sb->operations && sb->operations->sync_fs) {
    ret = sb->operations->sync_fs(sb, wait);
    if (ret)
      return ret;
  }

  /* Sync all dirty inodes */
  spin_lock(&sb->inode_states_lock);
  list_for_each_entry_safe(inode, next, &sb->dirty_inodes,
                           i_state_list_node) {
    if (sb->operations && sb->operations->write_inode) {
      spin_unlock(&sb->inode_states_lock);
      ret = sb->operations->write_inode(inode, wait);
      spin_lock(&sb->inode_states_lock);

      if (ret == 0 && wait) {
        /* Move from dirty list to LRU list */
        list_del(&inode->i_state_list_node);
        inode->i_state &= ~I_DIRTY;
        list_add(&inode->i_state_list_node, &sb->clean_inodes);
      }

      if (ret)
        break;
    }
  }
  spin_unlock(&sb->inode_states_lock);

  return ret;
}

/**
 * generic_shutdown_super - Generic superblock shutdown
 * @sb: Superblock to shut down
 *
 * Generic implementation for unmounting a filesystem.
 * Releases all inodes and drops the superblock.
 */
void generic_shutdown_super(struct super_block* sb) {
  struct inode *inode, *next;

  if (!sb)
    return;

  /* Write any dirty data */
  sync_filesystem(sb, 1);

  /* Free all inodes */
  spin_lock(&sb->all_inodes_lock);
  list_for_each_entry_safe(inode, next, &sb->all_inodes, i_sb_list_node) {
    spin_unlock(&sb->all_inodes_lock);

    /* Forcibly evict the inode */
    if (inode->i_state & I_DIRTY) {
      if (sb->operations && sb->operations->write_inode)
        sb->operations->write_inode(inode, 1);
    }

    if (sb->operations && sb->operations->evict_inode)
      sb->operations->evict_inode(inode);
    else
      clear_inode(inode);

    spin_lock(&sb->all_inodes_lock);
  }
  spin_unlock(&sb->all_inodes_lock);

  /* Free root dentry */
  if (sb->global_root_dentry) {
    dput(sb->global_root_dentry);
    sb->global_root_dentry = NULL;
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
int register_filesystem_types(void) {
  INIT_LIST_HEAD(&file_systems_list);
  spinlock_init(&file_systems_lock);
  int err;
  /* Register ramfs - our initial root filesystem */
  extern struct file_system_type ramfs_fs_type;
  err = register_filesystem(&ramfs_fs_type);
  if (err < 0)
    return err;

  /* Register other built-in filesystems */
  extern struct file_system_type hostfs_fs_type;
  err = register_filesystem(&hostfs_fs_type);
  if (err < 0)
    return err;

  return 0;
}

/**
 * register_filesystem - Register a new filesystem type
 * @fs: The filesystem type structure to register
 *
 * Adds a filesystem to the kernel's list of filesystems that can be mounted.
 * Returns 0 on success, error code on failure.
 * fs是下层文件系统静态定义的，所以不需要分配内存
 */
int register_filesystem(struct file_system_type* fs) {
  struct file_system_type* p;

  if (!fs || !fs->name)
    return -EINVAL;

  /* Initialize filesystem type */
  INIT_LIST_HEAD(&fs->fs_fslist_node);
  INIT_LIST_HEAD(&fs->superblock_list);

  /* Acquire lock for list manipulation */
  spinlock_init(&file_systems_lock);

  /* Check if filesystem already registered */
  list_for_each_entry(p, &file_systems_list, fs_fslist_node) {
    if (strcmp(p->name, fs->name) == 0) {
      /* Already registered */
      release_spinlock(&file_systems_lock);
      sprint("VFS: Filesystem %s already registered\n", fs->name);
      return -EBUSY;
    }
  }

  /* Add filesystem to the list (at the beginning for simplicity) */
  list_add(&fs->fs_fslist_node, &file_systems_list);

  spinlock_unlock(&file_systems_lock);
  sprint("VFS: Registered filesystem %s\n", fs->name);
  return 0;
}

/**
 * unregister_filesystem - Remove a filesystem type from the kernel's list
 * @fs: The filesystem type structure to unregister
 *
 * Removes a filesystem from the kernel's list of available filesystems.
 * Returns 0 on success, error code on failure.
 */
int unregister_filesystem(struct file_system_type* fs) {
  struct file_system_type* p;

  if (!fs || !fs->name)
    return -EINVAL;

  /* Acquire lock for list manipulation */
  acquire_spinlock(&file_systems_lock);

  /* Find filesystem in the list */
  list_for_each_entry(p, &file_systems_list, fs_fslist_node) {
    if (p == fs) {
      /* Found it - remove from the list */
      list_del(&p->fs_fslist_node);
      release_spinlock(&file_systems_lock);
      sprint("VFS: Unregistered filesystem %s\n", p->name);
      return 0;
    }
  }

  release_spinlock(&file_systems_lock);
  sprint("VFS: Filesystem %s not registered\n", fs->name);
  return -ENOENT;
}

/**
 * get_fs_type - Find a filesystem type by name
 * @name: The filesystem name to find
 *
 * Searches the list of registered filesystems for one with the given name.
 * Returns a pointer to the filesystem type structure or NULL if not found.
 */
struct file_system_type* get_fs_type(const char* name) {
  struct file_system_type* fs;

  if (!name)
    return NULL;

  acquire_spinlock(&file_systems_lock);

  list_for_each_entry(fs, &file_systems_list, fs_fslist_node) {
    if (strcmp(fs->name, name) == 0) {
      release_spinlock(&file_systems_lock);
      return fs;
    }
  }

  release_spinlock(&file_systems_lock);
  return NULL;
}

/**
 * mount_fs - Mount a filesystem
 * @type: Filesystem type
 * @flags: Mount flags
 * @dev_name: Device name (can be NULL for virtual filesystems)
 * @data: Filesystem-specific mount options
 *
 * Mounts a filesystem of the specified type.
 *
 * Returns the superblock on success, ERR_PTR on failure
 */
struct super_block* mount_fs(struct file_system_type* type, int flags,
                             const char* dev_name, void* data) {
  struct super_block* sb;
  int error;
  dev_t dev_id = 0; /* Default to 0 for virtual filesystems */

  if (unlikely(!type || !type->mount))
    return ERR_PTR(-ENODEV);

  /* Get device ID if we have a device name */
  if (dev_name && *dev_name) {
    error = lookup_dev_id(dev_name, &dev_id);
    if (error)
      return ERR_PTR(error);
  }

  /* Get or allocate superblock */
  sb = get_superblock(type, dev_id, data);
  if (!sb)
    return ERR_PTR(-ENOMEM);

  /* Set flags */
  sb->superblock_flags = flags;

  /* If this is a new superblock (no root yet), initialize it */
  if (sb->global_root_dentry == NULL) {
    /* Call fill_super if available */
    if (type->fill_super) {
      error = type->fill_super(sb, data, flags);
      if (error) {
        drop_super(sb);
        return ERR_PTR(error);
      }
    }
    /* Or call mount if fill_super isn't available */
    else if (type->mount) {
      /* This is a fallback - ideally all filesystems would
       * implement fill_super instead */
      struct super_block* new_sb;
      new_sb = type->mount(type, flags, dev_name, data);
      if (IS_ERR(new_sb)) {
        drop_super(sb);
        return new_sb;
      }
      /* This would need to handle merging the superblocks
       * but it's a non-standard path */
      drop_super(sb);
      sb = new_sb;
    }
  }

  /* Increment active count */
  grab_super(sb);

  return sb;
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