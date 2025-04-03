#pragma once

// #include <kernel/vfs.h>
#include "forward_declarations.h"
#include <kernel/mm/vma.h>
#include <kernel/util.h>

extern struct hashtable inode_hashtable;


struct inode* icache_lookup(struct superblock* sb, uint64 ino);
uint32 icache_hash(const void* key);
void icache_insert(struct inode* inode);
void icache_delete(struct inode* inode);
void* icache_getkey(struct list_node* node);
int32 icache_equal(const void* k1, const void* k2);
int32 icache_init(void);

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
	uint32 ia_valid;
	fmode_t ia_mode;
	uid_t ia_uid;
	gid_t ia_gid;
	loff_t ia_size;
	struct timespec ia_atime;
	struct timespec ia_mtime;
	struct timespec ia_ctime;
};

/* Forward declarations */
struct inode_operations;

struct inode_key {
	struct superblock* sb;
	uint64 ino;
};
/*
 * Inode structure - core of the filesystem
 */
struct inode {
	/* Identity */
	fmode_t i_mode; /* File type and permissions */
	uid_t i_uid;    /* Owner user ID */
	gid_t i_gid;    /* Owner group ID */
	dev_t i_rdev;   /* Device number (for special files) */

	/* File attributes */
	loff_t i_size; /* File size in bytes */
	// Replace existing timestamp fields with nanosecond precision
	struct timespec i_atime; /* Last access time */
	struct timespec i_mtime; /* Last modification time */
	struct timespec i_ctime; /* Last status change time */
	struct timespec i_btime; /* Creation time (birth time) */
	uint32 i_nlink;          /* Number of hard links */
	blkcnt_t i_blocks;       /* Number of blocks allocated */

	/* Memory management */
	struct addrSpace* i_mapping; /* Associated address space */

	/* Filesystem information */
	union{
		struct inode_key i_key; /* Key for hash table */
		struct{
			struct superblock* i_superblock;    /* Superblock */
			uint64 i_ino;   /* Inode number */
		};
	};



	struct list_node i_s_list_node;     /* Superblock list of inodes */
	struct list_node i_state_list_node; /* For ONE state list (LRU/dirty/IO) */

	/* Add hash table support */
	struct list_node i_hash_node; /* For hash table linkage */

	/* Operations */
	const struct inode_operations* i_op; /* Inode operations */
	const struct file_operations* i_fop; /* Default file operations */

	/* Reference counting and locking */
	atomic_t i_refcount;  /* Reference count */
	spinlock_t i_lock; /* Protects changes to inode */

	/* State tracking */
	uint64 i_state; /* Inode state flags */

	/* File system specific data */
	void* i_fs_info; /* Filesystem-specific data */

	/* Dentry management */
	struct list_head i_dentryList; /* List of dentries for this inode */
	                               /*只需要服务活跃的dentry，用来同步*/
	                               /*它们之间的状态，不用写回磁盘*/
	spinlock_t i_dentryList_lock;

	/* Block mapping */
	sector_t* i_data; /* Block mapping array */
	// // Quota fields
	// struct quota_info {
	//     spinlock_t dq_lock;
	//     struct dquot *dq_user;     /* User quota */
	//     struct dquot *dq_group;    /* Group quota */
	//     struct dquot *dq_project;  /* Project quota */
	// } i_quota;
};


/*dentry called executer functions*/
struct dentry* inode_lookup(struct inode* dir, struct dentry* dentry, uint32 lookup_flags);
int32 inode_mknod(struct inode* dir, struct dentry* dentry, mode_t mode, dev_t dev);
int32 inode_mkdir(struct inode* dir, struct dentry* dentry, mode_t mode);
int32 inode_rmdir(struct inode*, struct dentry*);
// Implementation functions for extended attributes
int32 inode_setxattr(struct inode* inode, const char* name, const void* value, size_t size, int32 flags);
ssize_t inode_getxattr(struct inode* inode, const char* name, void* value, size_t size);
ssize_t inode_listxattr(struct inode* inode, char* list, size_t size);
int32 inode_removexattr(struct inode* inode, const char* name);

struct inode_operations {
	/* File operations */
	struct dentry* (*lookup)(struct inode*, struct dentry*, uint32 lookup_flags);
	struct inode* (*create)(struct inode*, struct dentry*, fmode_t, bool);
	int32 (*link)(struct dentry*, struct inode*, struct dentry*);
	int32 (*unlink)(struct inode*, struct dentry*);
	int32 (*symlink)(struct inode*, struct dentry*, const char*);
	int32 (*mkdir)(struct inode*, struct dentry*, fmode_t);
	int32 (*rmdir)(struct inode*, struct dentry*);
	int32 (*mknod)(struct inode*, struct dentry*, fmode_t, dev_t);
	int32 (*rename)(struct inode*, struct dentry*, struct inode*, struct dentry*, uint32);

	/* Extended attribute operations */
	int32 (*setxattr)(struct dentry*, const char*, const void*, size_t, int32);
	ssize_t (*getxattr)(struct dentry*, const char*, void*, size_t);
	ssize_t (*listxattr)(struct dentry*, char*, size_t);
	int32 (*removexattr)(struct dentry*, const char*);

	/* Special file operations */
	int32 (*readlink)(struct dentry*, char*, int32);
	int32 (*get_link)(struct dentry*, struct inode*, struct path*);
	int32 (*permission)(struct inode*, int32);
	int32 (*get_acl)(struct inode*, int32);
	// int32 (*set_acl)(struct inode*, struct posix_acl*, int32);
	int32 (*setattr)(struct dentry*, struct iattr*);
	int32 (*getattr)(const struct path*, struct kstat*, uint32, uint32);
	int32 (*fiemap)(struct inode*, struct fiemap_extent_info*, uint64, uint64);

	/* Block operations */
	int32 (*get_block)(struct inode*, sector_t, struct buffer_head*, int32 create);
	sector_t (*bmap)(struct inode*, sector_t);
	void (*truncate_blocks)(struct inode*, loff_t size);

	/* Direct I/O support */
	int32 (*direct_IO)(struct kiocb*, struct io_vector_iterator*);
	// ACL operations
	// Memory mapping operations
	vm_fault_t (*page_fault)(struct vm_area_struct*, struct vm_fault*);
	uint64 (*get_unmapped_area)(struct file*, uint64, uint64, uint64, uint64);

	// POSIX specific operations
	int32 (*atomic_open)(struct inode*, struct dentry*, struct file*, unsigned open_flag, umode_t create_mode);
	int32 (*tmpfile)(struct inode*, struct dentry*, umode_t);
	// int32 (*dentry_open)(struct dentry *, struct file *, const struct cred *);
};

/*
 * inode_helpers
 */
int32 inode_cache_init(void);
struct inode* inode_acquire(struct superblock* sb, uint64 ino);
struct inode* inode_ref(struct inode* inode);
void inode_unref(struct inode* inode);



/* Inode lookup and creation */
int32 inode_permission(struct inode* inode, int32 mask);
/* Reference counting */

/*供下层文件系统调用，在IO读写后通知进程*/
int unlock_new_inode(struct inode* inode);
void wake_up_inode(struct inode* inode);

/* Inode state management */
void inode_setDirty(struct inode* inode);
int32 inode_sync(struct inode* inode, int32 wait);
int32 inode_sync_metadata(struct inode* inode, int32 wait);
/* Permission checking */
// int32 generic_permission(struct inode *inode, int32 mask);

/* Utility functions */
/**
 * inode_isBad - Check if an inode is invalid
 * @inode: inode to check
 *
 * Returns true if the specified inode has been marked as bad.
 */
static inline int32 inode_isBad(struct inode* inode) { return (!inode || inode->i_op == NULL); }
int32 inode_checkPermission(struct inode* inode, int32 mask);

// Add to inode.h or extend existing declarations
#define XATTR_CREATE 0x1  /* Create attribute if it doesn't exist */
#define XATTR_REPLACE 0x2 /* Replace attribute if it exists */



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
#define MAY_EXEC 0x0001      /* Execute permission */
#define MAY_WRITE 0x0002     /* Write permission */
#define MAY_READ 0x0004      /* Read permission */
#define MAY_APPEND 0x0008    /* Append-only permission */
#define MAY_ACCESS 0x0010    /* Check for existence */
#define MAY_OPEN 0x0020      /* Check permission for open */
#define MAY_CHDIR 0x0040     /* Check permission to use as working directory */
#define MAY_EXEC_MMAP 0x0080 /* Check exec permission for mmap PROT_EXEC */

/* Combined permissions for common operations */
#define MAY_LOOKUP (MAY_EXEC)                 /* For path traversal */
#define MAY_READLINK (MAY_READ)               /* For reading symlinks */
#define MAY_READ_WRITE (MAY_READ | MAY_WRITE) /* For both read and write */
#define MAY_CREATE (MAY_WRITE | MAY_EXEC)     /* For creating new files */
#define MAY_DELETE (MAY_WRITE | MAY_EXEC)     /* For deleting files */

// ACL types
#define ACL_TYPE_ACCESS (0x0000)  /* POSIX access ACL */
#define ACL_TYPE_DEFAULT (0x0001) /* POSIX default ACL */

bool inode_isReadonly(struct inode* inode);
bool inode_isImmutable(struct inode* inode);
bool inode_isDir(struct inode* inode);

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