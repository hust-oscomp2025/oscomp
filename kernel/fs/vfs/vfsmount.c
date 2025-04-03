#include <kernel/types.h>
#include <kernel/util.h>
#include <kernel/vfs.h>
#include <kernel/mmu.h>

/* Global mount list */
struct list_head mount_list;
spinlock_t mount_lock;

/* Mount point hash table */
struct hashtable mount_hashtable;
/* Forward declarations */
static uint32 mount_hash_func(const void* key);
static void* mount_get_key(struct list_head* node);
static int32 mount_key_equals(const void* key1, const void* key2);



static void __mount_free(struct vfsmount* mnt);
static int32 __mount_hash(struct vfsmount* mnt);
static void __mount_unhash(struct vfsmount* mnt);





/**
 * mount_hash_func - Hash function for mount points
 * @key: The path key to hash
 *
 * Returns a hash value for the path
 */
static uint32 mount_hash_func(const void* key) {
	const struct path* path = (const struct path*)key;
	uint64 hash;

	if (!path) return 0;

	/* Combine dentry and mount pointers for a good hash */
	hash = (uint64)(uintptr_t)path->dentry;
	hash = hash * 31 + (uint64)(uintptr_t)path->mnt;

	return (uint32)hash;
}

/**
 * mount_get_key - Extract path key from mount hash node
 * @node: Hash node (embedded in vfsmount structure)
 *
 * Returns the path structure used as key
 */
static void* mount_get_key(struct list_head* node) {
	struct vfsmount* mnt = container_of(node, struct vfsmount, mnt_hash_node);
	return &mnt->mnt_path;
}

/**
 * mount_key_equals - Compare two mount point paths
 * @k1: First path
 * @k2: Second path
 *
 * Returns 1 if paths are equal, 0 otherwise
 */
static int32 mount_key_equals(const void* k1, const void* k2) {
	const struct path* p1 = (const struct path*)k1;
	const struct path* p2 = (const struct path*)k2;

	return (p1->dentry == p2->dentry && p1->mnt == p2->mnt) ? 1 : 0;
}

/**
 * mcache_init - Initialize the mount subsystem
 */
void mcache_init(void) {
	/* Initialize global mount list */
	INIT_LIST_HEAD(&mount_list);
	spinlock_init(&mount_lock);

	/* Initialize hash table */
	hashtable_setup(&mount_hashtable, 256, 75, mount_hash_func, mount_get_key, mount_key_equals);
}

/**
 * mount_ref - Increment mount reference count
 */
struct vfsmount* mount_ref(struct vfsmount* mnt) {
	if (mnt) { atomic_inc(&mnt->mnt_refcount); }
	return mnt;
}

/**
 * mount_unref - Decrement mount reference count
 */
void mount_unref(struct vfsmount* mnt)
{
    if (!mnt)
        return;
        
    if (atomic_dec_and_test(&mnt->mnt_refcount)) {
        /* Last reference - free the mount */
        
        /* Remove from hash table if present */
        if (!list_empty(&mnt->mnt_hash_node))
            hashtable_remove(&mount_hashtable, &mnt->mnt_hash_node);
            
        /* Remove from global list */
        spinlock_lock(&mount_lock);
        list_del(&mnt->mnt_node_global);
        spinlock_unlock(&mount_lock);
        
        /* Remove from superblock list */
        if (mnt->mnt_superblock) {
            spinlock_lock(&mnt->mnt_superblock->s_list_mounts_lock);
            list_del(&mnt->mnt_node_superblock);
            spinlock_unlock(&mnt->mnt_superblock->s_list_mounts_lock);
        }
        
        /* Remove from parent's child list */
        if (!list_empty(&mnt->mnt_child_node))
            list_del(&mnt->mnt_child_node);
            
        /* Release references */
        if (mnt->mnt_root)
            dentry_unref(mnt->mnt_root);
        if (mnt->mnt_path.dentry)
            dentry_unref(mnt->mnt_path.dentry);
        if (mnt->mnt_path.mnt)
            mount_unref(mnt->mnt_path.mnt);
            
        /* Free the structure */
        __mount_free(mnt);
    }
}


/**
 * __mount_free - Free a vfsmount structure
 * @mnt: Mount to free
 */
static void __mount_free(struct vfsmount* mnt)
{
    if (!mnt)
        return;
    
    /* Free device name if present */
    if (mnt->mnt_devname)
        kfree(mnt->mnt_devname);
    
    /* Free the structure */
    kfree(mnt);
}


/**
 * mount_add - Add a mount to the mount tree
 * @newmnt: New mount to add
 * @mountpoint: Where to mount it
 * @flags: Mount flags
 * 
 * Returns 0 on success, error code on failure
 */
int32 mount_add(struct vfsmount* newmnt, struct path* mountpoint, int32 flags)
{
    struct vfsmount* parent;
    int32 error;
    
    if (!newmnt || !mountpoint)
        return -EINVAL;
    
    /* Check if the mountpoint is already mounted */
    parent = path_lookupMount(mountpoint);
    if (parent)
        return -EBUSY;
    
    /* Set up mountpoint */
    newmnt->mnt_path.dentry = dentry_ref(mountpoint->dentry);
    newmnt->mnt_path.mnt = mount_ref(mountpoint->mnt);
    
    /* Add to parent's child list */
    if (mountpoint->mnt) {
        list_add(&newmnt->mnt_child_node, &mountpoint->mnt->mnt_child_list);
    }
    
    /* Add to hash table */
    error = __mount_hash(newmnt);
    if (error) {
        dentry_unref(newmnt->mnt_path.dentry);
        mount_unref(newmnt->mnt_path.mnt);
        newmnt->mnt_path.dentry = NULL;
        newmnt->mnt_path.mnt = NULL;
        list_del_init(&newmnt->mnt_child_node);
        return error;
    }
    
    return 0;
}


/**
 * __mount_hash - Add a mount to the hash table
 * @mnt: Mount to add
 * 
 * Returns 0 on success, negative error on failure
 */
int32 __mount_hash(struct vfsmount* mnt)
{
    if (!mnt || !mnt->mnt_path.dentry)
        return -EINVAL;
    
    /* Add to hash table */
    return hashtable_insert(&mount_hashtable, &mnt->mnt_hash_node);
}


/**
 * __mount_unhash - Remove a mount from the hash table
 * @mnt: Mount to remove
 */
void __mount_unhash(struct vfsmount* mnt)
{
    if (!mnt)
        return;
    
    /* Remove from hash table */
    hashtable_remove(&mount_hashtable, &mnt->mnt_hash_node);
}



/**
 * mount_bind - Create a bind mount from a source path to a target path
 * @source_path: Source path object to bind from
 * @flags: Mount flags
 *
 * Creates a bind mount by duplicating the source mount point with a new
 * target location. This makes the contents of source_path visible at the
 * mount_point location.
 *
 * Returns: The new vfsmount on success, ERR_PTR on failure
 */
struct vfsmount* mount_bind(struct path* source_path, uint64 flags) {
    struct vfsmount* newmnt;
    
    if (!source_path || !source_path->dentry || !source_path->mnt)
        return ERR_PTR(-EINVAL);
    
    /* Allocate a new vfsmount structure */
    newmnt = kmalloc(sizeof(struct vfsmount));
    if (!newmnt)
        return ERR_PTR(-ENOMEM);
    
    memset(newmnt, 0, sizeof(struct vfsmount));
    
    /* Initialize lists */
    INIT_LIST_HEAD(&newmnt->mnt_node_superblock);
    INIT_LIST_HEAD(&newmnt->mnt_node_global);
    INIT_LIST_HEAD(&newmnt->mnt_hash_node);
    INIT_LIST_HEAD(&newmnt->mnt_node_namespace);
    INIT_LIST_HEAD(&newmnt->mnt_child_list);
    INIT_LIST_HEAD(&newmnt->mnt_child_node);
    
    /* Set up the bind mount */
    newmnt->mnt_root = dentry_ref(source_path->dentry);
    newmnt->mnt_superblock = source_path->mnt->mnt_superblock;
    newmnt->mnt_flags = (source_path->mnt->mnt_flags & ~MS_REC) | (flags & MS_REC);
    
    /* Determine if this should be a recursive bind mount */
    if (flags & MS_REC) {
        /* For recursive mounts, we would need to duplicate the entire mount tree.
           This is simplified in this implementation. A proper implementation
           would clone each submount recursively. */
    }
    
    /* Initialize refcount */
    atomic_set(&newmnt->mnt_refcount, 1);
    
    /* Update mount superblock reference */
    if (newmnt->mnt_superblock) {
        spinlock_lock(&newmnt->mnt_superblock->s_list_mounts_lock);
        list_add(&newmnt->mnt_node_superblock, &newmnt->mnt_superblock->s_list_mounts);
        spinlock_unlock(&newmnt->mnt_superblock->s_list_mounts_lock);
    }
    
    /* Add to global mount list */
    spinlock_lock(&mount_lock);
    list_add(&newmnt->mnt_node_global, &mount_list);
    spinlock_unlock(&mount_lock);
    
    /* Copy device name if needed */
    if (source_path->mnt->mnt_devname) {
        newmnt->mnt_devname = kmalloc(strlen(source_path->mnt->mnt_devname) + 1);
        if (newmnt->mnt_devname)
            strcpy(newmnt->mnt_devname, source_path->mnt->mnt_devname);
    }
    
    return newmnt;
}

/**
 * remount - Remount a filesystem with new options
 * @mount_path: Path to the existing mount point
 * @flags: New mount flags
 * @data: Filesystem-specific mount data
 *
 * Changes the mount flags and options of an existing mount.
 * This is typically used to change between read-only and read-write modes.
 *
 * Returns: 0 on success, negative error code on failure
 */
int32 remount(struct path* mount_path, uint64 flags,const void* data) {
    struct vfsmount* mnt;
    struct superblock* sb;
    int32 remount_flags;
    int32 ret = 0;
    
    if (!mount_path)
        return -EINVAL;
    
    /* Get the mount point corresponding to the path */
    mnt = path_lookupMount(mount_path);
    if (!mnt) 
        return -EINVAL;
    
    sb = mnt->mnt_superblock;
    if (!sb || !sb->s_operations || !sb->s_operations->remount)
        return -ENOSYS;
    
    /* Combine existing mount flags with requested changes */
    remount_flags = mnt->mnt_flags;
    
    /* Update read-only status if requested */
    if (flags & MS_RDONLY)
        remount_flags |= MS_RDONLY;
    else 
        remount_flags &= ~MS_RDONLY;
    
    /* Handle other flag changes */
    if (flags & MS_NOSUID)
        remount_flags |= MS_NOSUID;
    if (flags & MS_NODEV)
        remount_flags |= MS_NODEV;
    if (flags & MS_NOEXEC)
        remount_flags |= MS_NOEXEC;
    if (flags & MS_NOATIME)
        remount_flags |= MS_NOATIME;
    if (flags & MS_NODIRATIME)
        remount_flags |= MS_NODIRATIME;
    if (flags & MS_RELATIME)
        remount_flags |= MS_RELATIME;
    
    /* Notify the filesystem about remounting */
    ret = sb->s_operations->remount(sb,mount_path, &remount_flags, data);
    if (ret)
        return ret;
    
    /* Update mount flags */
    mnt->mnt_flags = remount_flags;
    
    return 0;
}
