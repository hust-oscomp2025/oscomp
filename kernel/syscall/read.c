#include <kernel/sched.h>
#include <kernel/vfs.h>
#include <kernel/syscall/syscall.h>
#include <kernel/mmu.h>
#include <kernel/util.h>

int64 sys_read(int32 fd, void* buf, size_t count) {
	void* kbuf = kmalloc(count);
	if (!kbuf) return -ENOMEM;
	int ret = do_read(fd, kbuf, count);
	if (ret < 0) {
		kfree(kbuf);
		return ret;
	}
	copy_to_user((void __user*)buf, kbuf, ret);
	/* Implementation here */
	return ret;
}
/**
 * Kernel-internal implementation of read syscall
 */
int64 do_read(int32 fd, void* buf, size_t count) {
	struct file* filp = fdtable_getFile(current_task()->fdtable,fd);
	if (!filp) return -EBADF;

	ssize_t ret = file_read(filp, buf, count, &filp->f_pos);
	file_unref(filp);
	return ret;
}


/**
 * Read data from a file
 * @param filp: The file pointer
 * @param buf: Buffer to store read data
 * @param count: Number of bytes to read
 * @param ppos: Current file position pointer
 * @return Number of bytes read, or negative error code
 */
ssize_t file_read(struct file *filp, char *buf, size_t count, loff_t *ppos) {
    ssize_t ret = -EINVAL;
    
    // Check if file is valid and has read operation
    if (!filp || !filp->f_op)
        return -EBADF;
    
    // Check read permission
    if (!(filp->f_mode & FMODE_READ))
        return -EBADF;
    
    // If the file has a read method, call it
    if (filp->f_op->read)
        ret = filp->f_op->read(filp, buf, count, ppos);
    // Otherwise, try using read_iter if available
    else {
		return -ENOSYS;
    }
    
    // Update access time if read was successful
    if (ret > 0) {
        fsnotify_access(filp);
        update_atime(filp);
    }
    
    return ret;
}
