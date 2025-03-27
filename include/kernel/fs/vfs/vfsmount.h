#ifndef _VFSMOUNT_H
#define _VFSMOUNT_H

#include <kernel/types.h>
#include <kernel/util/atomic.h>
#include <kernel/util/list.h>

/**
 * Mount point structure
 */
struct vfsmount {
	struct dentry* mnt_root; /* Root of this mount, 在常规挂载下是当前fs的根目录，在镜像挂载下是映射来源路径*/

	int32 mnt_flags; /* Mount flags */

	int32 mnt_id; /* Unique identifier for this mount */
	/* Mount point location */
	struct dentry* mnt_mountpoint; /* Dentry where this fs is mounted */

	/* List management */
	struct superblock* mnt_superblock;   /* Superblock of this mount */
	struct list_node mnt_node_superblock; /* Link in sb->s_list_mounts list */


	struct list_node mnt_node_global; /* Link in global mount list */

	/* Namespace mount list linkage */
	struct mnt_namespace* mnt_ns; /* Namespace containing this mount */
	struct list_node mnt_node_namespace;
	struct vfsmount* mnt_parent;        /* Parent mount point */
	struct list_node mnt_node_parent;   /* Link in parent's mnt_list_children */
	struct list_head mnt_list_children; /* List of children mounts */


	/* Reference counting */
	atomic_t mnt_refcount; /* Reference count */

	/* Device info */
	const char* mnt_devname; /* Device name */
};



/*
 * Mount registry and management
 */

/* Initialize mount system */
void init_mount_hash(void);

/* Mount management */
int32 do_mount(const char* dev_name, const char* path, const char* fstype, uint64 flags,
             void* data);

/* Unmount operations */
int32 do_umount(struct vfsmount* mnt, int32 flags);

/* Reference counting */
struct vfsmount* get_mount(struct vfsmount* mnt);
void put_mount(struct vfsmount* mnt);

/* Mount point lookup */
struct vfsmount* lookup_vfsmount(struct dentry* dentry);
struct vfsmount* lookup_mnt(struct path* path);

/* Mount traversal */
int32 iterate_mounts(int32 (*f)(struct vfsmount*, void*), void* arg, struct vfsmount* root);


#endif