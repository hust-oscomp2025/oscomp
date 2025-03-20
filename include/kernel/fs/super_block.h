#ifndef SUPER_BLOCK_H
#define SUPER_BLOCK_H
#include <kernel/types.h>
#include <util/list.h>
#include <util/spinlock.h>

struct fs_type;
struct super_operations;
struct dentry;

/* Superblock structure representing a mounted filesystem */
struct super_block {
	spinlock_t sb_lock; // Lock protecting the superblock

	/* Filesystem identification */
	unsigned int sb_magic;             // Magic number identifying filesystem
	dev_t sb_device_id;                // Device identifier
	unsigned long sb_blocksize;      // Block size in bytes
	unsigned long sb_blocksize_bits; // Block size bits (log2 of blocksize)

	/* Root of the filesystem */
	struct dentry* sb_global_root_dentry; // Root dentry

	/* Filesystem information and operations */
	struct fs_type* sb_fstype;      // Filesystem type
	struct list_node sb_node_fstype; // Instances of this filesystem

	void* sb_fs_specific; // Filesystem-specific information

	/* Master list - all inodes belong to this superblock */
	struct list_head sb_list_all_inodes; // List of inodes belonging to this sb
	spinlock_t sb_list_all_inodes_lock;  // Lock for sb_list_all_inodes

	/* State lists - an inode is on exactly ONE of these at any time */
	struct list_head sb_list_clean_inodes; // Clean, unused inodes (for reclaiming)
	struct list_head sb_list_dirty_inodes; // Dirty inodes (need write-back)
	struct list_head sb_list_io_inodes;    // Inodes currently under I/O
	/* One lock for all state lists */
	spinlock_t sb_list_inode_states_lock; // Lock for all state lists

	/* Filesystem statistics */
	unsigned long sb_file_maxbytes; // Max file size
	int sb_nblocks;               // Number of blocks
	atomic_int sb_ninodes;          // Number of inodes

	/* Locking and reference counting */
	atomic_int sb_refcount; // Reference count: mount point count + open file count
	// int s_active;      // Active reference count: 用来做懒卸载，目前不需要

	/* Mount info */
	struct list_head sb_list_mounts; // List of mounts
	spinlock_t sb_list_mounts_lock;       // Lock for all state lists

	/* Flags */
	unsigned long sb_flags; // Mount flags

	/* Quotas */
	// struct quota_info s_dquot;       // Quota operations

	/* Time values */
	unsigned long time_granularity;  /* Time granularity in nanoseconds */
	time_t sb_time_min; // Earliest time the fs can represent
	time_t sb_time_max; // Latest time the fs can represent
	                   // 取决于文件系统自身的属性，例如ext4的时间戳范围是1970-2106

	const struct super_operations* sb_operations; // Superblock operations
};

/* Function prototypes */

struct super_block* get_superblock(struct fs_type* type, void* data);
void drop_super(struct super_block* sb);

/* File system types */
struct fs_type {
	const char* fs_name;
	int fs_flags;

	/* Fill in a superblock */
	int (*fs_fill_sb)(struct super_block* sb, void* data, int silent);
	struct super_block* (*fs_mount_sb)(struct fs_type*, int, const char*, void*);
	void (*fs_kill_sb)(struct super_block*);

	/* Inside fs_type structure */
	struct list_node fs_node_gfslist; /* Node for linking into global filesystem list */
	                                 // spinlock_t fs_fslist_node_lock;

	struct list_head fs_list_sb;
	spinlock_t fs_list_sb_lock;
};

int register_filesystem_types(void);
int register_filesystem(struct fs_type*);
int unregister_filesystem(struct fs_type*);
struct fs_type* get_fs_type(const char* name);

/**
 * User-facing filesystem statistics
 * Populated from kstatfs for the statfs() system call
 */
struct statfs {
	long f_type;   // Filesystem type
	long f_bsize;  // Block size
	long f_blocks; // Total blocks
	long f_bfree;  // Free blocks
	long f_bavail; // Available blocks
	long f_files;  // Total inodes
	long f_ffree;  // Free inodes
};

/* Mount flags */
#define MS_RDONLY 1        // Mount read-only
#define MS_NOSUID 2        // Ignore suid and sgid bits
#define MS_NODEV 4         // Disallow access to device special files
#define MS_NOEXEC 8        // Disallow program execution
#define MS_SYNCHRONOUS 16  // Writes are synced at once
#define MS_REMOUNT 32      // Remount with different flags
#define MS_MANDLOCK 64     // Allow mandatory locks on this FS
#define MS_DIRSYNC 128     // Directory modifications are synchronous
#define MS_NOATIME 1024    // Do not update access times
#define MS_NODIRATIME 2048 // Do not update directory access times

// ... existing code ...

/* Superblock operations supported by all filesystems */
struct super_operations {
	/* Inode lifecycle management */
	struct inode* (*alloc_inode)(struct super_block* sb);
	void (*destroy_inode)(struct inode* inode);
	void (*dirty_inode)(struct inode* inode);

	/* Inode I/O operations */
	int (*write_inode)(struct inode* inode, int wait);
	int (*read_inode)(struct inode* inode);
	void (*evict_inode)(struct inode* inode);
	void (*drop_inode)(struct inode* inode);
	void (*delete_inode)(struct inode* inode);

	/* Superblock management */
	int (*sync_fs)(struct super_block* sb, int wait);
	int (*freeze_fs)(struct super_block* sb);
	int (*unfreeze_fs)(struct super_block* sb);
	int (*statfs)(struct super_block* sb, struct statfs* statfs);
	int (*remount_fs)(struct super_block* sb, int* flags, char* data);
	void (*umount_begin)(struct super_block* sb);

	/* Superblock lifecycle */
	void (*put_super)(struct super_block* sb);
	int (*sync_super)(struct super_block* sb, int wait);

	/* Filesystem-specific clear operations */
	void (*__clear_inode)(struct inode* inode);
	int (*show_options)(struct seq_file* seq, struct dentry* root);
};

#endif
