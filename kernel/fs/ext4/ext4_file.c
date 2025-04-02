#include <kernel/fs/ext4_adaptor.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/sprint.h>
#include <kernel/types.h>
#include <kernel/util/string.h>
#include <kernel/vfs.h>

/**
 * ext4_file_llseek - Position the file offset
 * @filp: The file to seek on
 * @offset: The offset to seek to
 * @whence: The seek type (SEEK_SET, SEEK_CUR, SEEK_END)
 *
 * Returns: New position on success, negative error code on failure
 */
static loff_t ext4_file_llseek(struct file* filp, loff_t offset, int32 whence) {
	struct ext4_file* ext4_file = (struct ext4_file*)filp->f_private;
	int32 ret;
	uint64_t pos;

	if (!ext4_file) return -EBADF;

	switch (whence) {
	case SEEK_SET:
		pos = offset;
		break;
	case SEEK_CUR:
		pos = filp->f_pos + offset;
		break;
	case SEEK_END:
		pos = ext4_fsize(ext4_file);
		if (ret != EOK) return -EIO;
		pos += offset;
		break;
	default:
		return -EINVAL;
	}

	ret = ext4_fseek(ext4_file, pos, SEEK_SET);
	if (ret != EOK) return -EIO;

	filp->f_pos = pos;
	return pos;
}

/**
 * ext4_file_read - Read data from a file
 * @filp: File to read from
 * @buf: Buffer to read into
 * @len: Number of bytes to read
 * @ppos: File position (updated after read)
 *
 * Returns: Bytes read on success, negative error code on failure
 */
static ssize_t ext4_file_read(struct file* filp, char* buf, size_t len, loff_t* ppos) {
	struct ext4_file* ext4_file = (struct ext4_file*)filp->f_private;
	size_t bytes_read;
	int32 ret;

	if (!ext4_file) return -EBADF;

	if (*ppos != filp->f_pos) {
		ret = ext4_fseek(ext4_file, *ppos, SEEK_SET);
		if (ret != EOK) return -EIO;
		filp->f_pos = *ppos;
	}

	ret = ext4_fread(ext4_file, buf, len, &bytes_read);
	if (ret != EOK && ret != ENOENT) return -EIO;

	*ppos = filp->f_pos += bytes_read;
	return bytes_read;
}

/**
 * ext4_file_write - Write data to a file
 * @filp: File to write to
 * @buf: Buffer containing data to write
 * @len: Number of bytes to write
 * @ppos: File position (updated after write)
 *
 * Returns: Bytes written on success, negative error code on failure
 */
static ssize_t ext4_file_write(struct file* filp, const char* buf, size_t len, loff_t* ppos) {
	struct ext4_file* ext4_file = (struct ext4_file*)filp->f_private;
	size_t bytes_written;
	int32 ret;

	if (!ext4_file) return -EBADF;

	if (*ppos != filp->f_pos) {
		ret = ext4_fseek(ext4_file, *ppos, SEEK_SET);
		if (ret != EOK) return -EIO;
		filp->f_pos = *ppos;
	}

	ret = ext4_fwrite(ext4_file, buf, len, &bytes_written);
	if (ret != EOK) return -EIO;

	*ppos = filp->f_pos += bytes_written;
	return bytes_written;
}

/**
 * ext4_dir_iterate - Read directory entries
 * @filp: Directory file
 * @ctx: Directory context including position and callback
 *
 * Returns: 0 on success, negative error code on failure
 */
static int32 ext4_dir_iterate(struct file* filp, struct dir_context* ctx) {
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

/**
 * ext4_file_open - Open a file
 * @inode: Inode of file to open
 * @filp: File structure to initialize
 *
 * Returns: 0 on success, negative error code on failure
 */
static int32 ext4_file_open(struct inode* inode, struct file* filp) {
	struct ext4_file* ext4_file;
	int32 ret;
	char* path;

	// Allocate ext4_file structure
	ext4_file = kmalloc(sizeof(struct ext4_file));
	if (!ext4_file) return -ENOMEM;

	// Get full path from dentry
	path = dentry_allocRawPath(filp->f_dentry);
	if (!path) {
		kfree(ext4_file);
		return -EINVAL;
	}

	// Open the file
	if (S_ISDIR(inode->i_mode)) {
		struct ext4_dir* ext4_dir = (struct ext4_dir*)ext4_file;
		ret = ext4_dir_open(ext4_dir, path);
	} else {
		int ext4_flags = 0;

		// Convert VFS flags to lwext4 flags
		if (filp->f_mode & FMODE_READ) ext4_flags |= O_RDONLY;
		if (filp->f_mode & FMODE_WRITE) ext4_flags |= O_WRONLY;
		if ((filp->f_mode & (FMODE_READ | FMODE_WRITE)) == (FMODE_READ | FMODE_WRITE)) ext4_flags = O_RDWR;

		ret = ext4_fopen(ext4_file, path, ext4_flags);
	}

	kfree(path);

	if (ret != EOK) {
		kfree(ext4_file);
		return -EIO;
	}

	filp->f_private = ext4_file;
	return 0;
}

/**
 * ext4_file_release - Release a file
 * @inode: Inode of file to release
 * @filp: File structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int32 ext4_file_release(struct inode* inode, struct file* filp) {
	int32 ret = 0;

	if (filp->f_private) {
		if (S_ISDIR(inode->i_mode)) {
			struct ext4_dir* ext4_dir = (struct ext4_dir*)filp->f_private;
			ret = ext4_dir_close(ext4_dir);
		} else {
			struct ext4_file* ext4_file = (struct ext4_file*)filp->f_private;
			ret = ext4_fclose(ext4_file);
		}

		kfree(filp->f_private);
		filp->f_private = NULL;
	}

	return (ret == EOK) ? 0 : -EIO;
}

/**
 * ext4_file_fsync - Synchronize file
 * @filp: File to sync
 * @start: Start offset (unused)
 * @end: End offset (unused)
 * @datasync: Only sync data, not metadata if true
 *
 * Returns: 0 on success, negative error code on failure
 */
static int32 ext4_file_fsync(struct file* filp, loff_t start, loff_t end, int32 datasync) {
	struct ext4_file* ext4_file = (struct ext4_file*)filp->f_private;
	int32 ret;

	if (!ext4_file) return -EBADF;

	ret = ext4_fsync(ext4_file);
	return (ret == EOK) ? 0 : -EIO;
}

/**
 * ext4_file_mmap - Memory map a file
 * @filp: File to map
 * @vma: Virtual memory area descriptor
 *
 * Returns: 0 on success, negative error code on failure
 */
static int32 ext4_file_mmap(struct file* filp, struct vm_area_struct* vma) {
	// For simplicity, delegate to generic file mmap handler
	return generic_file_mmap(filp, vma);
}

/**
 * Ext4 file operations structure
 */
const struct file_operations ext4_file_operations = {
    .llseek = ext4_file_llseek,
    .read = ext4_file_read,
    .write = ext4_file_write,
    .read_iter = NULL,  // Could implement with ext4_file_read
    .write_iter = NULL, // Could implement with ext4_file_write
    .iterate = ext4_dir_iterate,
    .iterate_shared = ext4_dir_iterate, // Same as non-shared for now
    .open = ext4_file_open,
    .flush = NULL, // Optional
    .release = ext4_file_release,
    .fsync = ext4_file_fsync,
    .mmap = ext4_file_mmap,
    .unlocked_ioctl = NULL, // Optional
    .fasync = NULL,         // Optional
    .fallocate = NULL       // Optional
};

/**
 * Ext4 directory operations structure
 */
const struct file_operations ext4_dir_operations = {
    .llseek = ext4_file_llseek,
    .read = NULL,  // Directories can't be read directly
    .write = NULL, // Directories can't be written directly
    .iterate = ext4_dir_iterate,
    .iterate_shared = ext4_dir_iterate,
    .open = ext4_file_open,
    .release = ext4_file_release,
    .fsync = ext4_file_fsync,
};