#ifndef SUPER_BLOCK_H
#define SUPER_BLOCK_H
#include <kernel/fs/inode.h>
#include <kernel/fs/vfs.h>
#include <kernel/types.h>
#include <util/list.h>
#include <util/spinlock.h>

struct file_system_type;
struct super_operations;
struct dentry;

/* Superblock structure representing a mounted filesystem */
struct super_block {
  /* Filesystem identification */
  unsigned int s_magic;           // Magic number identifying filesystem
  dev_t s_dev;                    // Device identifier
  unsigned long s_blocksize;      // Block size in bytes
  unsigned long s_blocksize_bits; // Block size bits (log2 of blocksize)

  /* Root of the filesystem */
  struct dentry *s_root; // Root dentry

  /* Filesystem information and operations */
  struct file_system_type *s_type;     // Filesystem type
  const struct super_operations *s_op; // Superblock operations
  void *s_fs_info;                     // Filesystem-specific information

  /* Master list - all inodes belong to this superblock */
  struct list_head s_inode_list; // List of inodes belonging to this sb
  spinlock_t s_inode_lock;       // Lock for s_inode_list

  /* State lists - an inode is on exactly ONE of these at any time */
  struct list_head s_inode_lru_list;   // Clean, unused inodes (for reclaiming)
  struct list_head s_inode_dirty_list; // Dirty inodes (need write-back)
  struct list_head s_inode_io_list;    // Inodes currently under I/O
  /* One lock for all state lists */
  spinlock_t s_inode_states_lock; // Lock for all state lists

  /* Filesystem statistics */
  unsigned long s_maxbytes; // Max file size
  int s_nblocks;            // Number of blocks
  int s_ninodes;            // Number of inodes

  /* Locking and reference counting */
  spinlock_t s_lock; // Lock protecting the superblock
  int s_count;       // Reference count
  int s_active;      // Active reference count

  /* Mount info */
  struct list_head s_mounts;    // List of mounts
  spinlock_t s_mounts_lock; // Lock for all state lists

	/* List of instances */
  struct list_head s_instances; // Instances of this filesystem

  /* Flags */
  unsigned long s_flags; // Mount flags

  /* Quotas */
  // struct quota_info s_dquot;       // Quota operations

  /* Time values */
  time_t s_time_min; // Earliest time the fs can represent
  time_t s_time_max; // Latest time the fs can represent
};

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
  struct inode *(*alloc_inode)(struct super_block *sb);
  void (*destroy_inode)(struct inode *inode);
  void (*dirty_inode)(struct inode *inode);

  /* Inode I/O operations */
  int (*write_inode)(struct inode *inode, int wait);
  int (*read_inode)(struct inode *inode);
  void (*evict_inode)(struct inode *inode);
  void (*drop_inode)(struct inode *inode);
  void (*delete_inode)(struct inode *inode);

  /* Superblock management */
  int (*sync_fs)(struct super_block *sb, int wait);
  int (*freeze_fs)(struct super_block *sb);
  int (*unfreeze_fs)(struct super_block *sb);
  int (*statfs)(struct super_block *sb, struct statfs *statfs);
  int (*remount_fs)(struct super_block *sb, int *flags, char *data);
  void (*umount_begin)(struct super_block *sb);

  /* Superblock lifecycle */
  void (*put_super)(struct super_block *sb);
  int (*sync_super)(struct super_block *sb, int wait);

  /* Filesystem-specific clear operations */
  void (*__clear_inode)(struct inode *inode);
  int (*show_options)(struct seq_file *seq, struct dentry *root);
};

/* Function prototypes */

struct super_block *sget(struct file_system_type *type, void *data);
void drop_super(struct super_block *sb);

#endif

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