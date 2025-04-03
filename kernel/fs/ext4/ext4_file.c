#include <kernel/fs/ext4_adaptor.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/util/print.h>
#include <kernel/types.h>
#include <kernel/util/string.h>
#include <kernel/vfs.h>

// EXT4表示兼容层方法。

/**
 * EXT4_dir_iterate - Read directory entries
 * @filp: Directory file
 * @ctx: Directory context including position and callback
 *
 * Returns: 0 on success, negative error code on failure
 */
static int32 EXT4_dir_iterate(struct file* filp, struct dir_context* ctx) {
	struct ext4_dir* ext4_dir = (struct ext4_dir*)filp->f_private;
	const ext4_direntry* entry;

	if (!ext4_dir) return -EBADF;

	// 简单地迭代目录条目
	while ((entry = ext4_dir_entry_next(ext4_dir)) != NULL) {
		// 确定条目类型
		unsigned char entry_type = DT_UNKNOWN;

		// 转换ext4的inode类型到dirent类型
		switch (entry->inode_type) {
		case EXT4_DE_REG_FILE:
			entry_type = DT_REG;
			break;
		case EXT4_DE_DIR:
			entry_type = DT_DIR;
			break;
		case EXT4_DE_CHRDEV:
			entry_type = DT_CHR;
			break;
		case EXT4_DE_BLKDEV:
			entry_type = DT_BLK;
			break;
		case EXT4_DE_FIFO:
			entry_type = DT_FIFO;
			break;
		case EXT4_DE_SOCK:
			entry_type = DT_SOCK;
			break;
		case EXT4_DE_SYMLINK:
			entry_type = DT_LNK;
			break;
		default:
			entry_type = DT_UNKNOWN;
		}

		// 调用回调函数，使用简单递增的位置索引
		if (!ctx->actor(ctx, (const char*)entry->name, entry->name_length,
		                ctx->pos++, // 简单地递增位置
		                entry->inode, entry_type)) {
			// 回调返回false，退出循环
			break;
		}
	}

	return 0;
}

// 修改 EXT4_file_open 接收 flags 和 mode
static int32 EXT4_file_open(struct file* file, int32 flags)
{
    ext4_file* ext4_f;
    int32 ret;
    char* path;
    
    /* 验证参数 */
    if (!file || !file->f_inode)
        return -EINVAL;
    
    /* 分配ext4文件结构 */
    ext4_f = kmalloc(sizeof(ext4_file));
    if (!ext4_f)
        return -ENOMEM;
    
    memset(ext4_f, 0, sizeof(ext4_file));
    
    /* 从dentry获取相对挂载点的路径 */
	path = dentry_allocPath2Mount(file->f_dentry);
    if (!path) {
        kfree(ext4_f);
        return -ENOMEM;
    }
    
    /* 使用传递的 flags 而不是从 file->f_mode 解析 */
    /* 在这里可以使用 mode 参数 */
    
    /* 使用lwext4库的fopen2函数打开文件 */
    ret = ext4_fopen2(ext4_f, path, flags);    
    if (ret != EOK) {
		kfree(ext4_f);
		goto clean;
    }
    
    /* 保存文件位置 */
    file->f_pos = ext4_f->fpos;
    
    /* 将ext4_file存储在file的私有数据中 */
    file->f_private = ext4_f;

clean:
	kfree(path);
	return ret;

}


static int32 EXT4_dir_open(struct file* file, int32 flags)
{
    ext4_dir* ext4_d;
    int32 ret;
    char* path;
    
    /* Validate parameters */
    if (!file || !file->f_inode)
        return -EINVAL;
    
    /* Make sure it's a directory */
    if (!S_ISDIR(file->f_inode->i_mode))
        return -ENOTDIR;
    
    /* Allocate ext4 directory structure */
    ext4_d = kmalloc(sizeof(ext4_dir));
    if (!ext4_d)
        return -ENOMEM;
    
    memset(ext4_d, 0, sizeof(ext4_dir));
    
    /* Get path relative to mount point */
    path = dentry_allocPath2Mount(file->f_dentry);
    if (!path) {
        kfree(ext4_d);
        return -ENOMEM;
    }
    
    /* Open directory using lwext4 library */
    ret = ext4_dir_open(ext4_d, path);
    if (ret != EOK) {
        kfree(ext4_d);
        kfree(path);
        return ret;
    }
    
    /* Store directory handle in file's private data */
    file->f_private = ext4_d;
    
    /* Clean up path string */
    kfree(path);
    return 0;
}


 /**
  * Read from a file
  */
 static ssize_t EXT4_file_read(struct file *file, char *buf, size_t count, loff_t *pos)
 {
	 struct ext4_file *ext4_file = (struct ext4_file *)file->f_private;
	 size_t bytes_read;
	 
	 /* Set file position */
	 int ret = ext4_fseek(ext4_file, *pos, SEEK_SET);
	 if (ret != 0)
		 return -EIO;
	 
	 /* Read from file */
	 ret = ext4_fread(ext4_file, buf, count, &bytes_read);
	 if (ret != 0)
		 return -EIO;
	
	 
	 return bytes_read;
 }

 static ssize_t EXT4_file_write(struct file *file, const char *buf, size_t count, loff_t *pos)
 {
	 struct ext4_file *ext4_file = (struct ext4_file *)file->f_private;
	 size_t bytes_written;
	 
	 /* Set file position */
	 int ret = ext4_fseek(ext4_file, *pos, SEEK_SET);
	 if (ret != 0)
		 return -EIO;
	 
	 /* Write to file */
	 ret = ext4_fwrite(ext4_file, buf, count, &bytes_written);
	 if (ret != 0)
		 return -EIO;
	
	 
	 return bytes_written;
 }
 


/**
 * Ext4 file operations structure
 */
const struct file_operations ext4_file_operations = {
    .open = EXT4_file_open,
	.read = EXT4_file_read,
	//.write = ext4_file_write,

    // .llseek = ext4_file_llseek,
    // 
    // 
    // .read_iter = NULL,  // Could implement with ext4_file_read
    // .write_iter = NULL, // Could implement with ext4_file_write
    // .flush = NULL, // Optional
    // .release = ext4_file_release,
    // .fsync = ext4_file_fsync,
    // .mmap = ext4_mmap_file,
    // .unlocked_ioctl = NULL, // Optional
    // .fasync = NULL,         // Optional
    // .fallocate = NULL       // Optional
};

/**
 * Ext4 directory operations structure
 */
const struct file_operations ext4_dir_operations = {
    .open = EXT4_dir_open,

    // .llseek = ext4_file_llseek,
    // .read = NULL,  // Directories can't be read directly
    // .write = NULL, // Directories can't be written directly
    // .iterate = EXT4_dir_iterate,
    // .release = ext4_file_release,
    // .fsync = ext4_file_fsync,
};