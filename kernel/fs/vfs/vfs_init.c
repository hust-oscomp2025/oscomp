#include <kernel/fs/dentry.h>
#include <kernel/fs/file.h>
#include <kernel/fs/inode.h>

#include <kernel/fs/file_system_type.h>
#include <kernel/fs/namespace.h>
#include <kernel/fs/super_block.h>
#include <kernel/fs/vfs.h>
#include <kernel/mm/kmalloc.h>
#include <spike_interface/spike_utils.h>
#include <util/hashtable.h>
#include <kernel/types.h>


/* Global mount list */
static struct list_head mount_list;
static spinlock_t mount_lock;

/* Mount point hash table */
static struct hashtable mount_hashtable;


/**
 * vfs_init - Initialize the VFS subsystem
 *
 * Initializes all the core VFS components in proper order.
 * Must be called early during kernel initialization before
 * any filesystem operations can be performed.
 */
int vfs_init(void)
{
    int err;
    init_mount_hash();

    /* Initialize the dcache subsystem */
    sprint("VFS: Initializing dentry cache...\n");
    err = init_dentry_hashtable();
    if (err < 0) {
        sprint("VFS: Failed to initialize dentry cache\n");
        return err;
    }
    
    /* Initialize the inode subsystem */
    sprint("VFS: Initializing inode cache...\n");
    err = inode_cache_init();
		if (err < 0) {
				sprint("VFS: Failed to initialize inode cache\n");
				return err;
		}



    /* Register built-in filesystems */
    sprint("VFS: Registering built-in filesystems...\n");
    err = register_filesystem_types();
    if (err < 0) {
        sprint("VFS: Failed to register filesystems\n");
        return err;
    }

    
    sprint("VFS: Initialization complete\n");
    return 0;
}


/**
 * hash_mountpoint - Hash a mountpoint
 */
static unsigned int hash_mountpoint(const void *key, unsigned int size) {
  const struct path *path = (const struct path *)key;
  unsigned long hash = (unsigned long)path->dentry;

  hash = hash * 31 + (unsigned long)path->mnt;
  return hash % size;
}

/**
 * mountpoint_equal - Compare two mountpoints
 */
static int mountpoint_equal(const void *k1, const void *k2) {
  const struct path *path1 = (const struct path *)k1;
  const struct path *path2 = (const struct path *)k2;

  return (path1->dentry == path2->dentry && path1->mnt == path2->mnt);
}

/**
 * init_mount_hash - Initialize the mount hash table
 */
void init_mount_hash(void) {
  hashtable_setup(&mount_hashtable, 256, 70, hash_mountpoint, mountpoint_equal);
	INIT_LIST_HEAD(&mount_list);
  spinlock_init(&mount_lock);
}

/**
 * lookup_mnt - Find a mount for a given path
 */
struct vfsmount *lookup_mnt(struct path *path) {
  struct vfsmount *mnt = NULL;

  /* Look up in the mount hash table */
  mnt = hashtable_lookup(&mount_hashtable, path);
  return mnt;
}

/**
 * vfs_kern_mount - Mount a filesystem
 */
struct vfsmount *vfs_kern_mount(struct fs_type *type, int flags,
                                const char *name, void *data) {
  struct vfsmount *mnt;
  struct dentry *root;

  if (!type)
    return ERR_PTR(-EINVAL);

  /* Call the filesystem's mount method */
  root = type->fs_mount_sb(type, flags, name, data);
  if (IS_ERR(root))
    return ERR_CAST(root);

  /* Allocate a new mount structure */
  mnt = kmalloc(sizeof(struct vfsmount));
  if (!mnt) {
    dentry_put(root);
    return ERR_PTR(-ENOMEM);
  }

  /* Initialize the mount */
  mnt->mnt_root = root;
  mnt->mnt_superblock = root->d_superblock;
  mnt->mnt_flags = flags;
  atomic_set(&mnt->mnt_refcount, 1);
  mnt->mnt_devname = kstrdup(name, GFP_KERNEL);
  INIT_LIST_HEAD(&mnt->mnt_node_superblock);
  INIT_LIST_HEAD(&mnt->mnt_list_children);
  INIT_LIST_HEAD(&mnt->mnt_node_parent);

  /* Add to superblock's mount list */
  spin_lock(&mnt->mnt_superblock->sb_list_mounts_lock);
  list_add(&mnt->mnt_node_superblock, &mnt->mnt_superblock->sb_list_mounts);
  spin_unlock(&mnt->mnt_superblock->sb_list_mounts_lock);

  /* Add to global mount list */
  spin_lock(&mount_lock);
  list_add(&mnt->mnt_node_global, &mount_list);
  spin_unlock(&mount_lock);

  return mnt;
}

/**
 * get_mount - Increment mount reference count
 */
struct vfsmount *get_mount(struct vfsmount *mnt) {
  if (mnt) {
    atomic_inc(&mnt->mnt_refcount);
    return mnt;
  }
  return NULL;
}

/**
 * put_mount - Decrement mount reference count
 */
void put_mount(struct vfsmount *mnt) {
  if (!mnt)
    return;

  if (atomic_dec_and_test(&mnt->mnt_refcount)) {
    /* Last reference - free the mount */
    spin_lock(&mount_lock);
    list_del(&mnt->mnt_node_global);
    spin_unlock(&mount_lock);

    spin_lock(&mnt->mnt_superblock->sb_list_mounts_lock);
    list_del(&mnt->mnt_node_superblock);
    spin_unlock(&mnt->mnt_superblock->sb_list_mounts_lock);

    dentry_put(mnt->mnt_root);
    if (mnt->mnt_devname)
      kfree(mnt->mnt_devname);
    kfree(mnt);
  }
}