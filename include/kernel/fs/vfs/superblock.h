#ifndef _SUPERBLOCK_H
#define _SUPERBLOCK_H
//#include <kernel/fs/vfs/fstype.h>
#include <kernel/types.h>
#include <util/list.h>
#include <util/spinlock.h>

struct fstype;
struct superblock_operations;
struct dentry;
struct block_device;

/* Superblock structure representing a mounted filesystem */
struct superblock {


	/***********************file system type and mounts******************************/

	struct fstype* s_fstype;      // Filesystem type
	struct list_node s_node_fstype; // Instances of this filesystem
	void* s_fs_info; // Filesystem-specific information
	const struct superblock_operations* s_operations; // Superblock operations
	/* Mount info */
	struct list_head s_list_mounts; // List of mounts
	spinlock_t s_list_mounts_lock;       // Lock for all state lists
	/* Quotas */
	// struct quota_info s_dquot;       // Quota operations
	struct block_device* s_bdev; // Block device
	struct dentry* s_root; // global root dentry
	//struct dentry* s_dentry; // Root dentry



	/*********************** fs statistics******************************/
	unsigned int s_magic;             // Magic number identifying filesystem
	dev_t s_device_id;      // Device identifier, 目前简单通过对挂载路径做哈希得到
	unsigned long s_blocksize;      // Block size in bytes
	unsigned long s_blocksize_bits; // Block size bits (log2 of blocksize)
	unsigned int s_max_links;
	unsigned long s_file_maxbytes; // Max file size
	int s_nblocks;               // Number of blocks
	unsigned long s_time_granularity;  /* Time granularity in nanoseconds */
	time_t s_time_min; // Earliest time the fs can represent
	time_t s_time_max; // Latest time the fs can represent
	                   // 取决于文件系统自身的属性，例如ext4的时间戳范围是1970-2106
	// int s_active;      // Active reference count: 用来做懒卸载，目前不需要
	unsigned long s_flags; // 只由fs_fill_super决定，不允许用户修改

	/*********************** vfs variables ******************************/
	spinlock_t s_lock; // Lock protecting the superblock
	atomic_int s_refcount; // Reference count: mount point count + open file count
	atomic_int s_ninodes;          // Number of inodes


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
struct vfsmount* superblock_acquireMount(struct superblock* sb, int flags, const char* device_path);




// ... existing code ...

/* Superblock operations supported by all filesystems */
struct superblock_operations {
    // 分配文件系统特定的超级块信息
    void* (*alloc_fs_info)(void);
    // 释放文件系统特定的超级块信息
    void (*free_fs_info)(void *fs_info);
	/* Inode lifecycle management */
	struct inode* (*alloc_inode)(struct superblock* sb);
	void (*destroy_inode)(struct inode* inode);
	void (*dirty_inode)(struct inode* inode);

	/* Inode I/O operations */
	int (*write_inode)(struct inode* inode, int wait);
	int (*read_inode)(struct inode* inode);
	void (*evict_inode)(struct inode* inode);
	void (*drop_inode)(struct inode* inode);
	void (*delete_inode)(struct inode* inode);

	/* Superblock management */
	int (*sync_fs)(struct superblock* sb, int wait);
	int (*freeze_fs)(struct superblock* sb);
	int (*unfreeze_fs)(struct superblock* sb);
	int (*statfs)(struct superblock* sb, struct statfs* statfs);
	int (*remount_fs)(struct superblock* sb, int* flags, char* data);
	void (*umount_begin)(struct superblock* sb);

	/* Superblock lifecycle */
	void (*put_super)(struct superblock* sb);
	int (*sync_super)(struct superblock* sb, int wait);

	/* Filesystem-specific clear operations */
	void (*__clear_inode)(struct inode* inode);
	int (*show_options)(struct seq_file* seq, struct dentry* root);

	int (*get_block)(struct inode* inode, sector_t iblock, struct buffer_head* bh_result, int create);

    // 阶段1: 初始化前检查 - 验证设备和传入的参数
    int (*pre_mount_check)(struct superblock *sb, struct block_device *bdev, 
		void *mount_options, int flags);
    // // 阶段2: 读取文件系统元数据 - 验证文件系统格式并填充superblock
    // int (*fill_super)(struct superblock *sb, struct block_device *bdev, 
	// 	void *mount_options, int silent);
	// 这两个过程是fstype中实现的虚函数
	// // 阶段3: 文件系统特定初始化 - 分配缓存、建立根目录、初始化特殊结构
	// int (*fs_init)(struct superblock *sb);
    // 阶段4: 挂载点创建 - 创建vfsmount结构并完成挂载
    struct vfsmount* (*create_mount)(struct superblock *sb, int flags, 
		const char* device_path, void *mount_options);
    /* 卸载生命周期操作 */
    
    // 阶段1: 准备卸载 - 检查是否可以安全卸载
    int (*pre_unmount)(struct superblock *sb);
    
    // 阶段2: 同步文件系统数据 - 确保所有修改都已写入磁盘
    //int (*sync_fs)(struct superblock *sb, int wait);
    
    // 阶段3: 文件系统特定清理 - 释放文件系统特有资源
    int (*cleanup)(struct superblock *sb);
    
    // 阶段4: 强制卸载处理 - 处理强制卸载场景，返回0表示成功强制卸载
    int (*force_unmount)(struct superblock *sb);
};


struct statfs {
    __fsword_t f_type;    // Magic number identifying FS type
    __fsword_t f_bsize;   // Optimal transfer block size
    fsblkcnt_t f_blocks;  // Total data blocks in file system
    fsblkcnt_t f_bfree;   // Free blocks in FS
    fsblkcnt_t f_bavail;  // Free blocks for unprivileged users
    fsfilcnt_t f_files;   // Total file nodes (inodes)
    fsfilcnt_t f_ffree;   // Free file nodes
    fsid_t     f_fsid;    // Filesystem ID
    __fsword_t f_namelen; // Max length of filenames
    __fsword_t f_frsize;  // Fragment size
    __fsword_t f_flags;   // Mount flags
    __fsword_t f_spare[4]; // Padding / future use
};

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
