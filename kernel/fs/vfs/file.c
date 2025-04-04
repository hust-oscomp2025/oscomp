#include <kernel/fs/vfs/dentry.h>
#include <kernel/fs/vfs/file.h>
#include <kernel/fs/vfs/inode.h>
// #include <kernel/fs/vfs/namespace.h>
#include <kernel/fs/vfs/path.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/sched/process.h>
#include <kernel/sched/sched.h>
#include <kernel/types.h>
#include <kernel/util/qstr.h>
#include <kernel/util/string.h>
#include <kernel/vfs.h>

#include <kernel/util/print.h>
#include <asm-generic/fcntl.h>



struct file* file_ref(struct file* file) {
	if (!file) return NULL;
	if (atomic_read(&file->f_refcount) <= 0) { panic("file_ref: f_refcount is already 0\n"); }
	/* Increment reference count */
	atomic_inc(&file->f_refcount);
	return file;
}

/**
 * file_open - Open a file using a pre-resolved path
 * @path: Path to the file
 * @flags: Open flags
 * @mode: Creation mode
 */
int32 file_open(struct file* file, int32 flags) {
	if (!file) return -EINVAL;
	if (file->f_op && file->f_op->open) return file->f_op->open(file, flags);
	return 0;
}

/**
 * file_free - Close a file and clean up resources
 * @filp: Pointer to the file to close
 * @owner: Task that owns the file descriptor (can be NULL for kernel-owned
 * files)
 *
 * Closes a file, releases resources, updates task's file descriptor table
 * if owner is specified, and calls the file's release operation.
 *
 * Returns 0 on success or negative error code on failure.
 */
int32 file_free(struct file* filp) {
	int32 error = 0;
	int32 fd = -1;

	if (!filp) return -EINVAL;

	/* Release the file's regular resources */
	if (filp->f_op && filp->f_op->release) {
		/* Call the file-specific release operation */
		error = filp->f_op->release(filp);
		if (error) {
			kprintf("Error when fs releasing file: %d\n", error);
			return error;
		}
	}

	/* Release associated resources */
	path_destroy(&filp->f_path);
	/* Free the file structure itself */
	kfree(filp);

	return error;
}

/**
 * file_denyWrite
 * @file: File pointer
 */
int32 file_denyWrite(struct file* file) {
	if (!file) { return -EINVAL; }
	if (!file->f_inode || atomic_read(&file->f_refcount) <= 0) return -EBADF;

	/* Deny write access by setting the mode to read-only */
	spinlock_lock(&file->f_lock);
	file->f_mode &= ~FMODE_WRITE;
	spinlock_unlock(&file->f_lock);
	return 0;
}

/**
 * file_allowWrite
 * @file: File pointer
 *
 * Allows write access to the file by setting the mode to read-write.
 * Returns 0 on success or negative error code on failure.
 */
int32 file_allowWrite(struct file* file) {
	if (!file) { return -EINVAL; }
	if (!file->f_inode || atomic_read(&file->f_refcount) <= 0) return -EBADF;

	/* Allow write access by setting the mode to read-write */
	spinlock_lock(&file->f_lock);
	file->f_mode |= FMODE_WRITE;
	spinlock_unlock(&file->f_lock);
	return 0;
}

/**
 * file_unref - Decrease reference count on a file
 * @file: File pointer
 * @owner: Task that owns the file descriptor (can be NULL)
 *
 * Decrements the reference count on a file and releases
 * it if the count reaches zero, passing owner information
 * for proper cleanup.
 */
int32 file_unref(struct file* file) {
	CHECK_PTR_VALID(file, -EINVAL);

	if (atomic_dec_and_test(&file->f_refcount)) {
		/* Reference count reached zero, close the file */
		file_free(file);
		return 0;
	}
	return -EBADF;
	/* This should not happen, as we expect the reference count
	 * to be managed properly by the caller */
}

/**
 * file_llseek - Repositions the file offset
 * @file: File to seek within
 * @offset: File offset to seek to
 * @whence: Type of seek (SEEK_SET, SEEK_CUR, SEEK_END)
 *
 * This function updates a file's position according to the whence parameter:
 * - SEEK_SET: Sets position to offset from start of file
 * - SEEK_CUR: Sets position to current position plus offset
 * - SEEK_END: Sets position to end of file plus offset
 *
 * Returns the new file position on success, or a negative error code on failure.
 */
loff_t file_llseek(struct file* file, loff_t offset, int32 whence) {
	// struct kiocb kiocb;
	loff_t new_pos;

	/* Basic validation */
	if (!file || !file->f_inode) return -EINVAL;

	/* Check if file is seekable */
	if (!(file->f_mode & FMODE_LSEEK)) return -ESPIPE;

	/* Initialize kiocb */
	// init_kiocb(&kiocb, file);
	//  The only potential usage would be if the file system's specific llseek operation needed it

	/* Call file-specific llseek operation if available */
	if (file->f_op && file->f_op->llseek) return file->f_op->llseek(file, offset, whence);

	/* Generic implementation for simple files */
	spinlock_lock(&file->f_lock);

	switch (whence) {
	case SEEK_SET:
		new_pos = offset;
		break;
	case SEEK_CUR:
		new_pos = file->f_pos + offset;
		break;
	case SEEK_END:
		new_pos = file->f_inode->i_size + offset;
		break;
	default:
		spinlock_unlock(&file->f_lock);
		return -EINVAL;
	}

	/* Validate the resulting position */
	if (new_pos < 0) {
		spinlock_unlock(&file->f_lock);
		return -EINVAL;
	}

	/* Update file position */
	file->f_pos = new_pos;
	spinlock_unlock(&file->f_lock);

	return new_pos;
}

/**
 * file_sync - Synchronize a file's in-core state with storage
 * @file: File to synchronize
 * @datasync: Only flush user data if non-zero (not metadata)
 *
 * This function flushes all dirty pages associated with the file to disk
 * and updates the on-disk file metadata. If datasync is non-zero, only
 * the file data is synced (not metadata).
 *
 * Returns 0 on success or a negative error code on failure.
 */
int32 file_sync(struct file* file, int32 datasync) {
	struct inode* inode;
	int32 ret = 0;

	/* Basic validation */
	if (!file) return -EINVAL;

	inode = file->f_inode;
	if (!inode) return -EINVAL;

	/* Call file-specific fsync operation if available */
	if (file->f_op && file->f_op->fsync) {
		return file->f_op->fsync(file, 0, INT64_MAX, datasync);
	}

	/* Generic implementation - sync the inode */
	if (datasync)
		ret = inode_sync(inode, 1); /* Only sync data blocks */
	else
		ret = inode_sync_metadata(inode, 1); /* Sync data and metadata */

	return ret;
}

// /**
//  * file_readv - Read data from a file into multiple buffers
//  * @file: File to read from
//  * @vec: Array of io_vector structures
//  * @vlen: Number of io_vector structures
//  * @pos: Position in file to read from (updated on return)
//  */
// ssize_t file_readv(struct file* file, const struct io_vector* vec, uint64 vlen, loff_t* pos) {
// 	struct kiocb kiocb;
// 	struct io_vector_iterator iter;
// 	ssize_t ret;

// 	if (!file || !vec || !pos) return -EINVAL;

// 	/* Initialize kiocb */
// 	init_kiocb(&kiocb, file);
// 	kiocb.ki_pos = *pos;

// 	/* If we're using a position different from the file's current position */
// 	if (*pos != file->f_pos) kiocb.ki_flags |= KIOCB_NOUPDATE_POS;

// 	/* Setup the io_vector iterator */
// 	ret = setup_io_vector_iterator(&iter, vec, vlen);
// 	if (ret < 0) return ret;

// 	/* Use optimized read_iter if available */
// 	if (likely(file->f_op && file->f_op->read_iter)) {
// 		ret = file->f_op->read_iter(&kiocb, &iter);
// 	} else if (file->f_op && file->f_op->read) {
// 		/* Fall back to sequential reads */
// 		ret = 0;
// 		for (uint64 i = 0; i < vlen; i++) {
// 			ssize_t bytes;
// 			bytes = file->f_op->read(file, vec[i].iov_base, vec[i].iov_len, &kiocb.ki_pos);
// 			if (bytes < 0) {
// 				if (ret == 0) ret = bytes;
// 				break;
// 			}
// 			ret += bytes;
// 			if (bytes < vec[i].iov_len) break; /* Short read */
// 		}
// 	} else {
// 		ret = -EINVAL;
// 	}

// 	/* Update the position for the caller */
// 	if (ret > 0) *pos = kiocb.ki_pos;

// 	return ret;
// }

// /**
//  * file_writev - Write data from multiple buffers to a file
//  * @file: File to write to
//  * @vec: Array of io_vector structures
//  * @vlen: Number of io_vector structures
//  * @pos: Position in file to write to (updated on return)
//  */
// ssize_t file_writev(struct file* file, const struct io_vector* vec, uint64 vlen, loff_t* pos) {
// 	struct kiocb kiocb;
// 	struct io_vector_iterator iter;
// 	ssize_t ret;

// 	if (!file || !vec || !pos) return -EINVAL;

// 	/* Initialize kiocb */
// 	init_kiocb(&kiocb, file);
// 	kiocb.ki_pos = *pos;

// 	/* If we're using a position different from the file's current position */
// 	if (*pos != file->f_pos) kiocb.ki_flags |= KIOCB_NOUPDATE_POS;

// 	/* Setup the io_vector iterator */
// 	ret = setup_io_vector_iterator(&iter, vec, vlen);
// 	if (ret < 0) return ret;

// 	/* Handle append mode */
// 	if (file->f_flags & O_APPEND) {
// 		kiocb.ki_flags |= KIOCB_APPEND;
// 		kiocb.ki_pos = file->f_inode->i_size;
// 	}

// 	/* Use optimized write_iter if available */
// 	if (file->f_op && file->f_op->write_iter) {
// 		ret = file->f_op->write_iter(&kiocb, &iter);
// 	} else if (file->f_op && file->f_op->write) {
// 		/* Fall back to sequential writes */
// 		ret = 0;
// 		for (uint64 i = 0; i < vlen; i++) {
// 			ssize_t bytes;
// 			bytes = file->f_op->write(file, vec[i].iov_base, vec[i].iov_len, &kiocb.ki_pos);
// 			if (bytes < 0) {
// 				if (ret == 0) ret = bytes;
// 				break;
// 			}
// 			ret += bytes;
// 			if (bytes < vec[i].iov_len) break; /* Short write */
// 		}
// 	} else {
// 		ret = -EINVAL;
// 	}

// 	/* Update the position for the caller */
// 	if (ret > 0) {
// 		*pos = kiocb.ki_pos;

// 		/* Mark the inode as dirty if write was successful */
// 		inode_setDirty(file->f_inode);
// 	}

// 	return ret;
// }

/**
 * file_close - Close a file by file pointer
 * @file: File to close
 *
 * Decreases the reference count of the file and frees resources
 * when the count reaches zero.
 *
 * Returns: 0 on success, negative error code on failure
 */
int32 file_close(struct file* file) {
	if (!file) return -EINVAL;

	// For logging/debugging
	if (file->f_path.dentry && file->f_path.dentry->d_name->name) {
		// kprintf("Closing file: %s\n", file->f_path.dentry->d_name.name);
	}

	// Just delegate to file_unref which handles reference counting
	return file_unref(file);
}




/**
 * open_flags_to_fmode - 将 open() 标志转换为内部文件模式
 * @flags: 用户传入的打开标志
 *
 * 将用户空间的 open() 标志转换为内核的 fmode_t 表示
 * 
 * 返回: 相应的内部文件模式
 */
fmode_t open_flags_to_fmode(int32 flags)
{
    fmode_t fmode = 0;
    
    /* 访问模式转换 */
    switch (flags & O_ACCMODE) {
    case O_RDONLY:
        fmode = FMODE_READ;
        break;
    case O_WRONLY:
        fmode = FMODE_WRITE;
        break;
    case O_RDWR:
        fmode = FMODE_READ | FMODE_WRITE;
        break;
    }
    
    /* 特殊模式标志 */
    if (flags & O_APPEND)
        fmode |= FMODE_APPEND;
    
    if (flags & O_NONBLOCK)
        fmode |= FMODE_NONBLOCK;
    
    if (flags & O_DIRECT)
        fmode |= FMODE_DIRECT;
    
    if (flags & O_SYNC || flags & O_DSYNC)
        fmode |= FMODE_SYNC;
    
    if (flags & O_EXCL)
        fmode |= FMODE_EXCL;
    
    // if (flags & O_EXEC)
    //     fmode |= FMODE_EXEC;
    
    if (flags & O_PATH)
        fmode |= FMODE_PATH;
    
    if (flags & O_DIRECTORY)
        fmode |= FMODE_DIRECTORY;
    
    return fmode;
}

int32 open2lookup(int32 open_flags) {
    uint32 lookup_flags = 0;
    
    /* Handle symlink following */
    if (!(open_flags & O_NOFOLLOW)) {
        lookup_flags |= LOOKUP_FOLLOW;
    }
    
    /* Handle directory requirement */
    if (open_flags & O_DIRECTORY) {
        lookup_flags |= LOOKUP_DIRECTORY;
    }
    
    /* Handle file creation */
    if (open_flags & O_CREAT) {
        lookup_flags |= LOOKUP_CREATE;
    }
    
    /* Handle exclusive creation */
    if ((open_flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)) {
        lookup_flags |= LOOKUP_EXCL;
    }
    
    // /* Handle automounting if supported */
    // #ifdef LOOKUP_AUTOMOUNT
    // if (!(open_flags & O_NOAUTO)) {
    //     lookup_flags |= LOOKUP_AUTOMOUNT;
    // }
    // #endif
    
    return lookup_flags;
}


int32 file_iterate(struct file* file, struct dir_context* context) {
	if(!file) return -EINVAL;
	if(file->f_op && file->f_op->iterate) {
		return file->f_op->iterate(file, context);
	}
	return -ENOSYS;
}


/**
 * validate_open_flags - 验证打开文件的标志参数
 * @flags: 用户传入的打开标志
 *
 * 检查用户提供的 open() 标志是否有效且不冲突
 * 
 * 返回: 成功返回0，失败返回负的错误码
 */
int32 validate_open_flags(int32 flags)
{
    /* 检查访问模式 */
    int32 acc_mode = flags & O_ACCMODE;
    if (acc_mode != O_RDONLY && acc_mode != O_WRONLY && 
        acc_mode != O_RDWR)
        return -EINVAL;
    
    /* 检查标志组合的有效性 */
    
    /* O_CREAT、O_EXCL 和 O_TRUNC 的组合规则 */
    if ((flags & O_TRUNC) && acc_mode == O_RDONLY)
        return -EINVAL; // 只读模式下不能截断
    
    if ((flags & O_EXCL) && !(flags & O_CREAT))
        return -EINVAL; // O_EXCL 必须和 O_CREAT 一起使用
    
    /* 检查互斥标志 */
    if ((flags & O_DIRECTORY) && (flags & O_TRUNC))
        return -EISDIR; // 不能截断目录
    
    /* 检查是否使用了未实现的标志 */
    if (flags & ~VALID_OPEN_FLAGS)
        return -EINVAL;
    
    return 0; // 标志有效
}

bool file_isReadable(struct file* file) {
	if (!file || !file->f_inode || atomic_read(&file->f_refcount) <= 0) return false;
	return (file->f_mode & FMODE_READ) != 0;
}
bool file_isWriteable(struct file* file) {
	if (!file || !file->f_inode || atomic_read(&file->f_refcount) <= 0) return false;
	return (file->f_mode & FMODE_WRITE) != 0;
}