#ifndef _VFS_H_
#define _VFS_H_

#include <kernel/fs/vfs/dentry.h>
#include <kernel/fs/vfs/file.h>
#include <kernel/fs/vfs/inode.h>
// #include <kernel/fs/vfs/namespace.h>
#include <kernel/fs/vfs/addrspace.h>
#include <kernel/fs/vfs/buffer_head.h>
#include <kernel/fs/vfs/fiemap.h>
#include <kernel/fs/vfs/fstype.h>
#include <kernel/fs/vfs/io_vector.h>
//#include <kernel/fs/vfs/kiocb.h>
#include <kernel/fs/vfs/path.h>
#include <kernel/fs/vfs/superblock.h>
#include <kernel/fs/vfs/vfsmount.h>
#include <kernel/fs/vfs/fdtable.h>
#include <kernel/fs/vfs/fs_struct.h>
#include <kernel/fs/vfs/fdtable.h>


#include <kernel/types.h>
#include <kernel/util/list.h>
#include <kernel/util/qstr.h>

/* Forward declarations for VFS structures */
// struct file;
// struct inode;
// struct dentry;
// struct superblock;
struct vfsmount; /* Keep forward declaration for compatibility */
// struct path;
struct nameidata;

/* Path name length limits */
#define PATH_MAX 4096 /* Maximum path length */
#define NAME_MAX 255  /* Maximum filename length */

/* Seek types */
#define SEEK_SET 0 /* Set position from beginning of file */
#define SEEK_CUR 1 /* Set position from current */
#define SEEK_END 2 /* Set position from end of file */

/**
 * Directory entry in a directory listing
 */
struct dirent {
	uint64_t d_ino;          /* Inode number */
	uint64_t d_off;          /* Offset to next dirent */
	unsigned short d_reclen; /* Length of this dirent */
	unsigned char d_type;    /* File type */
	char d_name[];           /* File name (null-terminated) */
};

/* File types for d_type */
#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12

/**
 * Filename lookup parameters
 */
struct nameidata {
	struct path path;    /* Path found so far */
	struct qstr last;    /* Last component */
	struct inode* inode; /* Current inode */
	uint32 flags;        /* Lookup flags */
	int32 last_type;     /* Last component type */
};

/*
 * VFS core API functions
 */
/* Initialization functions */
int32 vfs_init(void);
struct file *vfs_alloc_file(const struct path *path, int32 flags, mode_t mode);


#define vfs_open file_open
#define vfs_close file_close
#define vfs_read file_read
#define vfs_write file_write
#define vfs_llseek file_llseek
#define vfs_readv file_readv
#define vfs_writev file_writev
#define vfs_fsync file_sync

// #define vfs_mkdir dentry_mkdir

struct vfsmount* vfs_kern_mount(struct fstype* type, int32 flags, const char* name,const void* data);
struct dentry* vfs_mkdir(struct dentry* parent, const char* name, fmode_t mode);
struct dentry* vfs_mknod(struct dentry* parent, const char* name, mode_t mode, dev_t dev);


/*****上面是调试充分的，下面是不可靠的************************** */
/* File operations */

int32 vfs_stat(const char*, struct kstat*);
int32 vfs_statfs(struct path*, struct kstatfs*);
int32 vfs_utimes(const char*, struct timespec*, int32);

/* Directory operations */
// int32 vfs_mkdir(struct inode*, struct dentry*, fmode_t);
int32 vfs_unlink(struct inode*, struct dentry*);
int32 vfs_rename(struct inode*, struct dentry*, struct inode*, struct dentry*, uint32);

/* Link operations */
int32 vfs_link(struct dentry*, struct inode*, struct dentry*, struct inode**);
int32 vfs_symlink(struct inode*, struct dentry*, const char*);

/* Permission checking */
int32 vfs_permission(struct inode*, int32);
int32 vfs_pathwalk(struct dentry* base_dentry, struct vfsmount* base_mnt, const char* path_str, uint32 flags, struct path* result);

/* File mode checking helpers */
static inline int32 is_dir(fmode_t mode) { return (mode & S_IFMT) == S_IFDIR; }
static inline int32 is_file(fmode_t mode) { return (mode & S_IFMT) == S_IFREG; }
static inline int32 is_symlink(fmode_t mode) { return (mode & S_IFMT) == S_IFLNK; }
int vfs_validate_flags(int flags);



/* 
 * Comprehensive list of lookup flags for path_lookup operations
 * These flags control how the VFS resolves paths during pathname lookup
 */

/*
 * Basic lookup control flags
 */
#define LOOKUP_FOLLOW        0x0001  /* Follow terminal symlinks instead of returning them */
#define LOOKUP_DIRECTORY     0x0002  /* Target must be a directory */
#define LOOKUP_AUTOMOUNT     0x0004  /* Traverse mount points automatically */
#define LOOKUP_PARENT        0x0010  /* Look up the parent directory (for creates, renames, etc.) */
#define LOOKUP_REVAL         0x0020  /* Check validity of cached dentries during lookup */
#define LOOKUP_RCU           0x0080  /* Use RCU for lookups - faster but read-only, no reference counting */
#define LOOKUP_OPEN          0x0100  /* Lookup is for open() */
#define LOOKUP_CREATE        0x0200  /* Lookup may create the last path component */
#define LOOKUP_EXCL          0x0400  /* Lookup with exclusive creation semantics */
#define LOOKUP_RENAME_TARGET 0x0800  /* Lookup is for a rename target */

/*
 * Access mode and operation flags
 */
#define LOOKUP_EXECUTE       0x1000  /* Permission checking should check execute permission */
#define LOOKUP_DIRECTORY_OR_SYMLINK 0x2000 /* Target must be a directory or a symlink */
#define LOOKUP_PATH          0x4000  /* Only care about path validity, not inode access */
#define LOOKUP_ACCESS        0x8000  /* Check access permissions during lookup */

/*
 * Special case flags
 */
#define LOOKUP_CACHED       0x10000  /* Only use cached entries */
#define LOOKUP_NOCASE       0x20000  /* Case-insensitive lookup */
#define LOOKUP_EMPTY        0x40000  /* Target must not exist (for exclusive creates) */
#define LOOKUP_XDEV         0x80000  /* Don't cross mount points */

/*
 * Internal state flags
 */
#define LOOKUP_JUMPED      		0x00100000  /* Path lookup crossed a mount point */
#define LOOKUP_ROOT        		0x00200000  /* Lookup relative to root (internal use) */
#define LOOKUP_BENEATH     		0x00400000  /* Don't escape specified directory */
#define LOOKUP_IN_ROOT     		0x00800000  /* Lookup in alternate root (chroot-like) */

/*
 * Negative lookup flags
 */
#define LOOKUP_NO_SYMLINKS 		0x01000000 /* Don't resolve any symlinks at all */
#define LOOKUP_NO_XDEV   	  	0x02000000 /* Don't cross mount points */
#define LOOKUP_NO_MAGICLINKS 	0x04000000 /* Don't handle "magic links" (/proc/self, etc) */
#define LOOKUP_NO_AUTOMOUNT 	0x08000000 /* Don't automount anything */
// 新增的LOOKUP_FLAGS
#define LOOKUP_CREATE_FILE 		0x10000000
#define LOOKUP_CREATE_DIR   	0x20000000
#define LOOKUP_CREATE_SYMLINK 	0x40000000

/*
 * Common combinations of lookup flags
 */
#define LOOKUP_NORMAL       (LOOKUP_FOLLOW) /* Standard flags for normal file lookup */
#define LOOKUP_OPEN_INTENT  (LOOKUP_OPEN | LOOKUP_CREATE | LOOKUP_FOLLOW) /* For open() */
#define LOOKUP_LINK_INTENT  (LOOKUP_PARENT) /* For link() */
#define LOOKUP_UNLINK_INTENT (LOOKUP_PARENT) /* For unlink() */
#define LOOKUP_RENAME_INTENT (LOOKUP_PARENT | LOOKUP_RENAME_TARGET) /* For rename() */


#endif /* _VFS_H_ */