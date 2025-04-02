#include <kernel/vfs.h>
#include <kernel/sched.h>
// 用一种可变长度的缓冲区来存储目录项
// 标准库会解释传出的buffer
/* Linux style dirent structure for compatibility */
struct linux_dirent {
    uint64 d_ino;             /* Inode number */
    int64  d_off;             /* Offset to next dirent */
    uint16 d_reclen;          /* Length of this dirent */
    uint8  d_type;            /* File type */
    char   d_name[1];         /* Filename (null-terminated) */
};

/* Getdents callback structure */
struct getdents_callback {
    struct dir_context ctx;    /* Must be first */
    char *current;            /* Current position in the buffer */
    size_t count;             /* Remaining buffer space */
};



int64 sys_getdents64(int32 fd, struct linux_dirent *dirp, size_t count) {
    struct file *file;
    struct getdents_callback buf;
    int ret;
    
    /* Validate parameters */
    if (!dirp)
        return -EFAULT;
    
    /* Get file from fd */
    file = fdtable_getFile(current_task()->fdtable, fd);
    if (!file)
        return -EBADF;
    
    /* Check if file is a directory */
    if (!S_ISDIR(file->f_inode->i_mode))
        return -ENOTDIR;
    
    /* Setup the callback structure */
    buf.ctx.actor = filldir;
    buf.ctx.pos = file->f_pos;
    buf.current = (char *)dirp;
    buf.count = count;
    
    /* Do the actual directory read */
    ret = do_getdents64(file, &buf.ctx);
    if (ret < 0)
        return ret;
    
    /* Update file position */
    file->f_pos = buf.ctx.pos;
    
    /* Return number of bytes written to user buffer */
    return count - buf.count;
}


/**
 * Internal implementation of directory reading
 */
int32 do_getdents64(struct file* file, struct dir_context* ctx) {
	int32 ret;

	/* Check if the file has an iterate method */
	if (!file->f_op || !file->f_op->iterate) return -ENOTDIR;

	/* Call the filesystem-specific iterate method */
	ret = file->f_op->iterate(file, ctx);

	return ret;
}




/* Internal filldir callback for directory iteration */
static int filldir(struct dir_context* ctx, const char* name, int namlen, loff_t offset, uint64 ino, unsigned int d_type) {
	struct linux_dirent* dirent;
	struct getdents_callback* buf = (struct getdents_callback*)ctx;
	unsigned int reclen = ALIGN(sizeof(struct linux_dirent) + namlen + 1, sizeof(uint64));

	/* Check if we have enough buffer space left */
	if (buf->count < reclen) return 0;

	/* Fill in the Linux dirent structure */
	dirent = (struct linux_dirent*)(buf->current);
	dirent->d_ino = ino;
	dirent->d_off = offset;
	dirent->d_reclen = reclen;
	dirent->d_type = d_type;
	strncpy(dirent->d_name, name, namlen);
	dirent->d_name[namlen] = '\0';

	/* Update buffer pointer and remaining count */
	buf->current += reclen;
	buf->count -= reclen;

	return 1; /* Continue iteration */
}
