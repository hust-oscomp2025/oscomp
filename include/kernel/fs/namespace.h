#ifndef _NAMESPACE_H
#define _NAMESPACE_H

#include <kernel/types.h>
#include <util/atomic.h>
#include <util/list.h>

/* Forward declarations */
struct super_block;
struct dentry;
struct path;

/* Mount flags */
#define MNT_RDONLY 1              /* Mount read-only */
#define MNT_NOSUID 2              /* Ignore suid and sgid bits */
#define MNT_NODEV 4               /* Disallow access to device special files */
#define MNT_NOEXEC 8              /* Disallow program execution */
#define MNT_RELATIME (1 << 21)    /* Update atime relative to mtime/ctime */
#define MNT_STRICTATIME (1 << 29) /* Always perform atime updates */

/**
 * Mount point structure
 */
struct vfsmount {
  struct dentry *mnt_root;    /* Root of this mount */
  struct super_block *mnt_sb; /* Superblock of this mount */
  int mnt_flags;              /* Mount flags */

  /* Mount point location */
  struct vfsmount *mnt_parent;   /* Parent mount point */
  struct dentry *mnt_mountpoint; /* Dentry where this fs is mounted */

  /* List management */
  struct list_head mnt_instance; /* Link in sb->s_mounts list */
  struct list_head mnt_mounts;   /* List of children mounts */
  struct list_head mnt_child;    /* Link in parent's mnt_mounts */
  struct list_head mnt_list;     /* Link in global mount list */

  /* Reference counting */
  atomic_t mnt_count; /* Reference count */

  /* Device info */
  const char *mnt_devname; /* Device name */
};

/*
 * Mount registry and management
 */

/* Initialize mount system */
void init_mount_hash(void);

/* Mount management */
struct vfsmount *vfs_kern_mount(struct file_system_type *type, int flags,
                                const char *name, void *data);
int do_mount(const char *dev_name, const char *path, const char *fstype,
             unsigned long flags, void *data);

/* Unmount operations */
int do_umount(struct vfsmount *mnt, int flags);

/* Reference counting */
struct vfsmount *get_mount(struct vfsmount *mnt);
void put_mount(struct vfsmount *mnt);

/* Mount point lookup */
struct vfsmount *lookup_vfsmount(struct dentry *dentry);
struct vfsmount *lookup_mnt(struct path *path);

/* Mount registry management */
void get_mnt_ns(struct mnt_namespace *ns);
void put_mnt_ns(struct mnt_namespace *ns);

/* Mount traversal */
bool is_mounted(struct dentry *dentry);
int iterate_mounts(int (*f)(struct vfsmount *, void *), void *arg,
                   struct vfsmount *root);

#endif /* _NAMESPACE_H */