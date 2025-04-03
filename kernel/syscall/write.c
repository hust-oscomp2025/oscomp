#include <kernel/sched.h>
#include <kernel/vfs.h>
#include <kernel/syscall/syscall.h>
#include <kernel/mmu.h>
#include <kernel/util.h>


int64 sys_write(int32 fd, const void* buf, size_t count) {
	void* kbuf = kmalloc(count);
	if (!kbuf) return -ENOMEM;
	if (copy_from_user(kbuf, buf, count)) {
		kfree(kbuf);
		return -EFAULT;
	}
	int ret = do_write(fd, kbuf, count);
	if (ret < 0) {
		kfree(kbuf);
		return ret;
	}
	kfree(kbuf);
	/* Implementation here */
	return ret;
}

/**
 * Kernel-internal implementation of write syscall
 */
ssize_t do_write(int32 fd, const void* buf, size_t count) {
	struct file* filp = fdtable_getFile(current_task()->fdtable,fd);
	if (!filp) return -EBADF;

	ssize_t ret = file_write(filp, buf, count, &filp->f_pos);
	file_unref(filp);
	return ret;
}


ssize_t file_write(struct file *filp, const char *buf, size_t count, loff_t *ppos) {
	ssize_t ret = -EINVAL;

	// Check if file is valid and has write operation
	if (!filp || !filp->f_op)
		return -EBADF;

	// Check write permission
	if (!(filp->f_mode & FMODE_WRITE))
		return -EBADF;

	// If the file has a write method, call it
	if (filp->f_op->write)
		ret = filp->f_op->write(filp, buf, count, ppos);
	else {
		ret = -EINVAL;
	}

	return ret;
}

