#include <kernel/fs/vfs/vfs.h>


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