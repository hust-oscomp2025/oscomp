#ifndef _VFSMOUNT_H
#define _VFSMOUNT_H

#include <kernel/types.h>
#include <util/atomic.h>
#include <util/list.h>

/**
 * Mount point structure
 */
struct vfsmount {
	struct dentry* mnt_root; /* Root of this mount, 在常规挂载下是当前fs的根目录，在镜像挂载下是映射来源路径*/

	int mnt_flags; /* Mount flags */

	int mnt_id; /* Unique identifier for this mount */
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
int do_mount(const char* dev_name, const char* path, const char* fstype, unsigned long flags,
             void* data);

/* Unmount operations */
int do_umount(struct vfsmount* mnt, int flags);

/* Reference counting */
struct vfsmount* get_mount(struct vfsmount* mnt);
void put_mount(struct vfsmount* mnt);

/* Mount point lookup */
struct vfsmount* lookup_vfsmount(struct dentry* dentry);
struct vfsmount* lookup_mnt(struct path* path);

/* Mount traversal */
bool is_mounted(struct dentry* dentry);
int iterate_mounts(int (*f)(struct vfsmount*, void*), void* arg, struct vfsmount* root);


#define MS_RDONLY       1     /* 挂载为只读 */
#define MS_NOSUID       2     /* 忽略 set-user-ID 和 set-group-ID */
#define MS_NODEV        4     /* 不解释设备文件 */
#define MS_NOEXEC       8     /* 禁止执行 */
#define MS_SYNCHRONOUS  16    /* 同步 I/O */
#define MS_REMOUNT      32    /* 改变挂载标志 */
#define MS_MANDLOCK     64    /* 允许强制锁 */
#define MS_DIRSYNC      128   /* 目录同步 */
#define MS_NOATIME      1024  /* 不更新访问时间 */
#define MS_NODIRATIME   2048  /* 不更新目录访问时间 */
#define MS_BIND         4096  /* 绑定挂载（bind mount）*/
#define MS_MOVE         8192  /* 移动挂载点 */
#define MS_REC          16384 /* 递归挂载（bind/move 时用）*/
#define MS_SILENT       32768 /* 静默挂载，不显示错误信息 */
#define MS_POSIXACL     (1<<16)  /* 启用 POSIX ACL */
#define MS_UNBINDABLE   (1<<17)
#define MS_PRIVATE      (1<<18)
#define MS_SLAVE        (1<<19)
#define MS_SHARED       (1<<20)
#define MS_RELATIME     (1<<21)
#define MS_KERNMOUNT    (1<<22)
#define MS_I_VERSION    (1<<23)
#define MS_STRICTATIME  (1<<24)
#define MS_LAZYTIME     (1<<25)

#endif