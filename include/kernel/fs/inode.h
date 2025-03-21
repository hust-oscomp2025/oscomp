#ifndef INODE_H
#define INODE_H

#include <kernel/fs/vfs.h>
#include <kernel/types.h>
#include <util/atomic.h>
#include <util/list.h>
#include <util/spinlock.h>

extern struct hashtable inode_hashtable;

/* File types */
#define S_IFMT 0170000   /* Mask for file type */
#define S_IFREG 0100000  /* Regular file */
#define S_IFDIR 0040000  /* Directory */
#define S_IFCHR 0020000  /* Character device */
#define S_IFBLK 0060000  /* Block device */
#define S_IFIFO 0010000  /* FIFO */
#define S_IFLNK 0120000  /* Symbolic link */
#define S_IFSOCK 0140000 /* Socket */

/* Permission bits */
#define S_ISUID 0004000 /* Set user ID on execution */
#define S_ISGID 0002000 /* Set group ID on execution */
#define S_ISVTX 0001000 /* Sticky bit */
#define S_IRWXU 0000700 /* User mask */
#define S_IRUSR 0000400 /* User read permission */
#define S_IWUSR 0000200 /* User write permission */
#define S_IXUSR 0000100 /* User execute permission */
#define S_IRWXG 0000070 /* Group mask */
#define S_IRGRP 0000040 /* Group read permission */
#define S_IWGRP 0000020 /* Group write permission */
#define S_IXGRP 0000010 /* Group execute permission */
#define S_IRWXO 0000007 /* Others mask */
#define S_IROTH 0000004 /* Others read permission */
#define S_IWOTH 0000002 /* Others write permission */
#define S_IXOTH 0000001 /* Others execute permission */

/* File type check macros */
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

/* Attribute flags for iattr */
#define ATTR_MODE (1 << 0)
#define ATTR_UID (1 << 1)
#define ATTR_GID (1 << 2)
#define ATTR_SIZE (1 << 3)
#define ATTR_ATIME (1 << 4)
#define ATTR_MTIME (1 << 5)
#define ATTR_CTIME (1 << 6)
#define ATTR_ATIME_SET (1 << 7)
#define ATTR_MTIME_SET (1 << 8)
#define ATTR_FORCE (1 << 9)

/**
 * struct iattr - attributes to be changed
 * @ia_valid: bitmask of attributes to change
 * @ia_mode: new file mode
 * @ia_uid: new owner uid
 * @ia_gid: new group id
 * @ia_size: new file size
 * @ia_atime: new access time
 * @ia_mtime: new modification time
 * @ia_ctime: new change time
 */
struct iattr {
	unsigned int ia_valid;
	fmode_t ia_mode;
	uid_t ia_uid;
	gid_t ia_gid;
	loff_t ia_size;
	struct timespec ia_atime;
	struct timespec ia_mtime;
	struct timespec ia_ctime;
};

/* Forward declarations */
struct dir_context;
struct kstat;
struct iattr;
struct fiemap_extent_info;
struct address_space_operations;
struct inode_operations;
struct radix_tree_root;

/* Inode hash key - combination of superblock and inode number */
struct inode_key {
	struct super_block* sb;
	unsigned long ino;
};

/*
 * Inode structure - core of the filesystem
 */
struct inode {
	/* Identity */
	fmode_t i_mode;      /* File type and permissions */
	uid_t i_uid;         /* Owner user ID */
	gid_t i_gid;         /* Owner group ID */
	unsigned long i_ino; /* Inode number */
	dev_t i_rdev;        /* Device number (for special files) */

	/* File attributes */
	loff_t i_size;           /* File size in bytes */
	struct timespec i_atime; /* Last access time */
	struct timespec i_mtime; /* Last modification time */
	struct timespec i_ctime; /* Last status change time */
	unsigned int i_nlink;    /* Number of hard links */
	blkcnt_t i_blocks;       /* Number of blocks allocated */

	/* Memory management */
	struct address_space {
		// struct inode *host;               /* Owning inode */
		struct radix_tree_root page_tree;             /* Page cache radix tree */
		spinlock_t tree_lock;                         /* Lock for tree manipulation */
		unsigned long nrpages;                        /* Number of total pages */
		const struct address_space_operations* a_ops; /* s_operations */
	}* i_mapping;                                     /* Associated address space */

	/* Filesystem information */
	struct super_block* i_superblock;   /* Superblock */
	struct list_head i_s_list_node;    /* Superblock list of inodes */
	struct list_head i_state_list_node; /* For ONE state list (LRU/dirty/IO) */

	/* Operations */
	const struct inode_operations* i_op; /* Inode operations */
	const struct file_operations* i_fop; /* Default file operations */

	/* Reference counting and locking */
	atomic_t i_count;  /* Reference count */
	spinlock_t i_lock; /* Protects changes to inode */

	/* State tracking */
	unsigned long i_state; /* Inode state flags */

	/* File system specific data */
	void* i_fs_info; /* Filesystem-specific data */

	/* Dentry management */
	struct list_head i_dentryList; /* List of dentries for this inode */
	                               /*只需要服务活跃的dentry，用来同步*/
	                               /*它们之间的状态，不用写回磁盘*/
	spinlock_t i_dentryList_lock; 
	

	/* Block mapping */
	sector_t* i_data; /* Block mapping array */
};

/*
 * Inode APIs
 */
int inode_cache_init(void);

struct inode* alloc_inode(struct super_block* sb);

/* Reference counting */
struct inode* grab_inode(struct inode* inode);
void put_inode(struct inode* inode);

/* Inode lookup and creation */
struct inode* get_inode(struct super_block* sb, unsigned long ino);

/* Inode state management */
void mark_inode_dirty(struct inode* inode);
void unlock_new_inode(struct inode* inode);

int sync_inode(struct inode* inode, int wait);
int sync_inode_metadata(struct inode* inode, int wait);
/* Permission checking */
// int generic_permission(struct inode *inode, int mask);

/* Utility functions */
int is_bad_inode(struct inode* inode);
int inode_permission(struct inode* inode, int mask);
int setattr_prepare(struct dentry* dentry, struct iattr* attr);
int notify_change(struct dentry* dentry, struct iattr* attr);

/*
 * Address space s_operations (page cache)
 */
struct address_space_operations {
	int (*readpage)(struct file*, struct page*);
	int (*writepage)(struct page*, struct writeback_control*);
	int (*readpages)(struct file*, struct address_space*, struct list_head*, unsigned);
	int (*writepages)(struct address_space*, struct writeback_control*);
	void (*invalidatepage)(struct page*, unsigned int);
	int (*releasepage)(struct page*, int);
	int (*direct_IO)(int, struct kiocb*, const struct io_vector*, loff_t, unsigned long);
};

/*
 * Inode operations
 */
struct inode_operations {
	/* File operations */
	struct dentry* (*lookup)(struct inode*, struct dentry*, unsigned int);
	struct inode* (*create)(struct inode*, struct dentry*, fmode_t, bool);
	int (*link)(struct dentry*, struct inode*, struct dentry*);
	int (*unlink)(struct inode*, struct dentry*);
	int (*symlink)(struct inode*, struct dentry*, const char*);
	int (*mkdir)(struct inode*, struct dentry*, fmode_t);
	int (*rmdir)(struct inode*, struct dentry*);
	int (*mknod)(struct inode*, struct dentry*, fmode_t, dev_t);
	int (*rename)(struct inode*, struct dentry*, struct inode*, struct dentry*, unsigned int);

	/* Extended attribute operations */
	int (*setxattr)(struct dentry*, const char*, const void*, size_t, int);
	ssize_t (*getxattr)(struct dentry*, const char*, void*, size_t);
	ssize_t (*listxattr)(struct dentry*, char*, size_t);
	int (*removexattr)(struct dentry*, const char*);

	/* Special file operations */
	int (*readlink)(struct dentry*, char*, int);
	int (*get_link)(struct dentry*, struct inode*, struct path*);
	int (*permission)(struct inode*, int);
	int (*get_acl)(struct inode*, int);
	int (*setattr)(struct dentry*, struct iattr*);
	int (*getattr)(const struct path*, struct kstat*, unsigned int, unsigned int);
	int (*fiemap)(struct inode*, struct fiemap_extent_info*, uint64, uint64);

	/* Block operations */
	int (*get_block)(struct inode*, sector_t, struct buffer_head*, int create);
	sector_t (*bmap)(struct inode*, sector_t);
	void (*truncate_blocks)(struct inode*, loff_t size);

	/* Direct I/O support */
	int (*direct_IO)(struct kiocb*, struct io_vector_iterator*);
};

/* Inode state flags */
#define I_DIRTY (1 << 0)          /* Inode is dirty - needs writing */
#define I_NEW (1 << 1)            /* Inode is newly created */
#define I_SYNC (1 << 2)           /* Sync is in progress for this inode */
#define I_REFERENCED (1 << 3)     /* Inode was recently accessed */
#define I_DIRTY_TIME (1 << 4)     /* Dirty timestamps only */
#define I_DIRTY_PAGES (1 << 5)    /* Dirty pages only */
#define I_FREEING (1 << 6)        /* Inode is being freed */
#define I_CLEAR (1 << 7)          /* Inode is being cleared */
#define I_DIRTY_SYNC (1 << 8)     /* Inode needs fsync */
#define I_DIRTY_DATASYNC (1 << 9) /* Data needs fsync */

/* Permission checking masks */
#define MAY_EXEC 0x0001   /* Execute permission */
#define MAY_WRITE 0x0002  /* Write permission */
#define MAY_READ 0x0004   /* Read permission */
#define MAY_APPEND 0x0008 /* Append-only permission */
#define MAY_ACCESS 0x0010 /* Check for existence */
#define MAY_OPEN 0x0020   /* Check permission for open */
#define MAY_CHDIR 0x0040  /* Check permission to use as working directory */

/* Combined permissions for common operations */
#define MAY_LOOKUP (MAY_EXEC)                 /* For path traversal */
#define MAY_READLINK (MAY_READ)               /* For reading symlinks */
#define MAY_READ_WRITE (MAY_READ | MAY_WRITE) /* For both read and write */
#define MAY_CREATE (MAY_WRITE | MAY_EXEC)     /* For creating new files */
#define MAY_DELETE (MAY_WRITE | MAY_EXEC)     /* For deleting files */

#endif /* INODE_H */

//        ┌─────────────┐
//        │             │
// ┌─────▶│   CLEAN    │◀─────┐
// │      │  (LRU)      │      │
// │      │             │      │
// │      └─────────────┘      │
// │             │             │
// │             │             │
// Write        Mark dirty     I/O completes
// completes     │             │
// │             │             │
// │             ▼             │
// │      ┌─────────────┐      │
// │      │             │      │
// └─────-│   DIRTY     │------┘
//        │             │
//        └─────────────┘
// 							 │
// 							 │
// 				   Start I/O
// 							 │
// 							 ▼
//				 ┌─────────────┐
// 			 	 │             │
// 		 		 │    I/O      │
//				 │             │
// 				 └─────────────┘