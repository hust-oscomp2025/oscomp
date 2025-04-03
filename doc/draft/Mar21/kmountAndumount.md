You've touched on an important conceptual distinction in filesystem mounting. The relationship between mount points and namespaces is fundamental to understanding why there's a `vfs_kern_mount` function.

## Relationship Between Mount Points and Namespaces

A **mount namespace** is a container for a set of mount points that form a filesystem hierarchy visible to processes in that namespace. In your implementation, this is represented by `struct mnt_namespace`.

Key points about this relationship:

1. **Isolation**: Different processes can have different mount namespaces, allowing them to see different filesystem hierarchies.

2. **Containment**: A mount namespace contains multiple `vfsmount` structures organized in a tree structure that represents the mounted filesystem hierarchy.

3. **Visibility control**: Mounts in one namespace are not visible to processes in other namespaces (unless shared).

In your code, you can see this relationship:

```c
struct mnt_namespace {
    struct vfsmount* root;        /* Root of the mount tree */
    struct list_head mount_list;  /* All mounts in this namespace */
    int mount_count;              /* Count of mounts */
    atomic_t count;               /* Reference count */
    uid_t owner;                  /* Owner info */
    spinlock_t lock;              /* Protection */
};
```

## Kernel-Mount vs. User-Mount

The distinction between `vfs_kern_mount` and regular mounts is about visibility and namespace membership:

1. **vfs_kern_mount (Kernel Mount)**:
   - Creates a mount point that is not initially attached to any namespace
   - Only visible to the kernel, not to user processes
   - Used for internal kernel operations or preparing mounts before making them visible

2. **do_mount (User Mount)**:
   - Takes a kernel mount and adds it to a process's mount namespace
   - Makes the mount visible to user processes
   - Implements the user-facing `mount(2)` system call

Here's how they're related in the mount lifecycle:

```
vfs_kern_mount()  -->  do_mount()  -->  Visible in user namespace
    |                   |
 Create mount      Add mount to
    |              namespace
 Not visible       |
 to users          Visible to users
```

## The Purpose of Keeping Them Separate

The separation serves several important functions:

1. **Bootstrapping**: During system initialization, you need to mount the root filesystem before namespaces are fully set up.

2. **Mount preparation**: Some complex mounting operations need to prepare a mount before deciding where to place it.

3. **Security and isolation**: By separating mount creation from namespace addition, the kernel can enforce security checks at the appropriate boundaries.

4. **Mount movement**: Mounts can be moved between namespaces without recreating them.

In your implementation, you can see in `do_mount()` that it's taking care of adding a mount to the current process's namespace:

```c
/* Add to current namespace */
if (current_task()->fs) {
    struct mnt_namespace* ns = current_task()->fs->mnt_ns;
    if (ns) {
        spin_lock(&ns->lock);
        list_add(&mnt->mnt_node_namespace, &ns->mount_list);
        ns->mount_count++;
        spin_unlock(&ns->lock);
    }
}
```

So to directly answer your question:
- `vfs_kern_mount`: Internal kernel-only mount, not in any namespace
- Regular mount (via `do_mount`): User-visible mount added to a namespace

This separation is a design pattern borrowed from Linux that provides flexibility in how filesystems are managed and presented to users.