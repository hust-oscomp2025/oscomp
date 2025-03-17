#include <kernel/fs/vfs.h>
#include <kernel/sched/sched.h>



inline struct mnt_namespace* grab_mnt_ns(struct mnt_namespace* ns) {
  if (ns) {
    atomic_inc(&ns->count);
  }
  return ns;
}

/**
 * put_mnt_ns - Decrement reference count on a mount namespace
 * @ns: Mount namespace
 * 
 * Drops a reference on the specified mount namespace.
 * When the last reference is dropped, the namespace is destroyed.
 */
void put_mnt_ns(struct mnt_namespace *ns)
{
    if (!ns)
        return;
        
    if (atomic_dec_and_test(&ns->count)) {
        /* Free all mounts in this namespace */
        struct vfsmount *mnt, *next;
        list_for_each_entry_safe(mnt, next, &ns->mount_list, mnt_node_global) {
            list_del(&mnt->mnt_node_global);
            put_mount(mnt);
        }
        
        /* Free the namespace structure itself */
        kfree(ns);
    }
}



/**
 * create_mnt_ns - Create a new mount namespace
 * @parent: Parent namespace to clone from (or NULL for empty)
 * 
 * Creates a new mount namespace, optionally cloning the contents
 * from an existing namespace.
 * 
 * Returns the new namespace or NULL on failure.
 */
struct mnt_namespace *create_mnt_ns(struct mnt_namespace *parent)
{
    struct mnt_namespace *ns;
    
    ns = kmalloc(sizeof(*ns));
    if (!ns)
        return NULL;
        
    /* Initialize the namespace */
    INIT_LIST_HEAD(&ns->mount_list);
    ns->mount_count = 0;
    atomic_set(&ns->count, 1);
    ns->owner = current_task()->euid;
    spinlock_init(&ns->lock);
    
    /* Clone parent namespace if provided */
    if (parent) {
        /* Implementation for cloning would go here */
        /* This requires a deep copy of the mount tree */
        /* For simplicity, just starting with an empty namespace */
    }
    
    return ns;
}