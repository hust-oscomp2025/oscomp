/*
 * RamFS - 内存文件系统
 * 一个简单的基于内存和页缓存的文件系统实现
 */

 #ifndef _RAMFS_H
 #define _RAMFS_H
 
 #include <kernel/types.h>
 #include <kernel/fs/vfs.h>
 #include <kernel/fs/super_block.h>
 
 /* 文件系统类型和魔数定义 */
 #define RAMFS_MAGIC     0x52414D46 /* "RAMF" */
 #define RAMFS_TYPE      2          /* 文件系统类型ID */
 
 /* 文件类型定义 */
 #define RAMFS_FILE      1
 #define RAMFS_DIR       2
 
 /**** 函数声明 ****/
 
 /* 文件系统初始化与注册 */
 int register_ramfs(void);
 struct device *init_ramfs_device(char *name);
 int ramfs_format_dev(struct device *dev);
 
 /* VFS接口函数 */
 struct super_block *ramfs_get_sb(struct device *dev);
 struct inode *ramfs_alloc_inode(struct super_block *sb);
 int ramfs_write_inode(struct inode *inode);
 
 #endif /* _RAMFS_H */