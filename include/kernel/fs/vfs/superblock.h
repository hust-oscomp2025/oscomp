#ifndef _SUPERBLOCK_H
#define _SUPERBLOCK_H
//#include <kernel/fs/vfs/fstype.h>
#include <kernel/types.h>
#include <kernel/fs/vfs/stat.h>
#include <kernel/util/list.h>
#include <kernel/util/spinlock.h>

struct fstype;
struct superblock_operations;
struct dentry;
struct block_device;
struct seq_file;

/* Superblock structure representing a mounted filesystem */
struct superblock {


	/***********************file system type and mounts******************************/

	/* Mount info */
	struct list_head s_list_mounts; // List of mounts
	spinlock_t s_list_mounts_lock;       // Lock for all state lists
	/* Quotas */
	// struct quota_info s_dquot;       // Quota operations
	struct dentry* s_root; // global root dentry
	//struct dentry* s_dentry; // Root dentry
	dev_t s_device_id;      // Device identifier, 目前简单通过对挂载路径做哈希得到
	struct block_device* s_bdev; // Block device

	/*********************** fs specified ******************************/
	struct fstype* s_fstype;      // Filesystem type
	struct list_node s_node_fstype; // Instances of this filesystem

	void* s_fs_info; // Filesystem-specific information

	uint32 s_magic;             // Magic number identifying filesystem
	uint64 s_blocksize;      // Block size in bytes
	uint64 s_blocksize_bits; // Block size bits (log2 of blocksize)
	uint32 s_max_links;
	uint64 s_file_maxbytes; // Max file size
	int32 s_nblocks;               // Number of blocks
	uint64 s_time_granularity;  /* Time granularity in nanoseconds */
	time_t s_time_min; // Earliest time the fs can represent
	time_t s_time_max; // Latest time the fs can represent
	                   // 取决于文件系统自身的属性，例如ext4的时间戳范围是1970-2106
	// int32 s_active;      // Active reference count: 用来做懒卸载，目前不需要
	uint64 s_flags; // 只由fs_fill_super决定，不允许用户修改
	const struct superblock_operations* s_operations; // Superblock operations

	/*********************** vfs variables ******************************/
	spinlock_t s_lock; // Lock protecting the superblock
	atomic_t s_refcount; // Reference count: mount point count + open file count
	atomic_t s_ninodes;          // Number of inodes


	/*********************** inode fields ******************************/
	/* Master list - all inodes belong to this superblock */
	struct list_head s_list_all_inodes; // List of inodes belonging to this sb
	spinlock_t s_list_all_inodes_lock;  // Lock for s_list_all_inodes

	/* State lists - an inode is on exactly ONE of these at any time */
	struct list_head s_list_clean_inodes; // Clean, unused inodes (for reclaiming)
	struct list_head s_list_dirty_inodes; // Dirty inodes (need write-back)
	struct list_head s_list_io_inodes;    // Inodes currently under I/O
	spinlock_t s_list_inode_states_lock; // Lock for all state lists

};

/* Function prototypes */
void superblock_put(struct superblock* sb);
struct vfsmount* superblock_acquireMount(struct superblock* sb, int32 flags, const char* device_path);
struct inode* superblock_createInode(struct superblock* sb, uint64 ino);




// ... existing code ...



#define ST_RDONLY       0x0001  // 文件系统是只读的
#define ST_NOSUID       0x0002  // 忽略 SUID 和 SGID 权限位
#define ST_NODEV        0x0004  // 禁止访问设备文件
#define ST_NOEXEC       0x0008  // 禁止执行程序
#define ST_SYNCHRONOUS  0x0010  // 所有写入操作都同步进行
#define ST_MANDLOCK     0x0040  // 支持强制锁（Mandatory locking）
#define ST_WRITE        0x0080  // 文件系统当前是可写的（不是标准 POSIX，但有些内核提供）
#define ST_APPEND       0x0100  // 支持 append-only 文件
#define ST_IMMUTABLE    0x0200  // 支持 immutable 文件
#define ST_NOATIME      0x0400  // 不更新访问时间
#define ST_NODIRATIME   0x0800  // 不更新目录访问时间
#define ST_RELATIME     0x1000  // 更新访问时间仅在 mtime 变动时












#endif
