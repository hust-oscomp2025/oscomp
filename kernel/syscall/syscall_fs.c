#include <kernel/vfs.h>
#include <kernel/mm/uaccess.h>
#include <kernel/sched/process.h>
#include <kernel/syscall/syscall.h>
#include <kernel/types.h>
#include <kernel/sprint.h>
#include <kernel/util/string.h>
#include <kernel/mm/kmalloc.h>

/**
 * sys_mount - Mount a filesystem
 * @source_user: User pointer to device name or source
 * @target_user: User pointer to mount point path
 * @fstype_user: User pointer to filesystem type
 * @flags: Mount flags
 * @data_user: User pointer to filesystem-specific data
 *
 * System call handler for mount(). Translates user addresses to kernel
 * addresses and calls VFS do_mount().
 *
 * Returns 0 on success, negative error code on failure
 */
int64 sys_mount(const char* source_user, const char* target_user, const char* fstype_user, uint64 flags, const void* data_user) {
	char* k_source = NULL;
	char* k_target = NULL;
	char* k_fstype = NULL;
	void* k_data = NULL;
	int32 ret = 0;

	// 验证用户指针
	if (!target_user) return -EFAULT;

	// 源设备可以为 NULL (例如 proc, sysfs)
	if (source_user) {
		k_source = user_to_kernel_str(source_user);
		if (!k_source) return -EFAULT;
	}

	// 挂载点路径 (必须提供)
	k_target = user_to_kernel_str(target_user);
	if (!k_target) {
		ret = -EFAULT;
		if (k_source) kfree(k_source);

		return ret;
	}

	// 文件系统类型 (必须提供)
	if (!fstype_user) {
		ret = -EINVAL;
		kfree(k_target);
		if (k_source) kfree(k_source);

		return ret;
	}

	k_fstype = user_to_kernel_str(fstype_user);
	if (!k_fstype) {
		ret = -EFAULT;
		kfree(k_target);
		if (k_source) kfree(k_source);

		return ret;
	}

	// 文件系统特定数据 (可选)
	if (data_user) {
		// 注意：这里我们处理的是字符串形式的挂载选项
		// 实际实现可能需要根据文件系统类型处理不同格式的数据
		k_data = user_to_kernel_str(data_user);
		// 数据可选，失败不会导致整个挂载失败
	}

	sprint("Mount: %s on %s type %s flags=0x%lx\n", k_source ? k_source : "(none)", k_target, k_fstype, flags);

	// 调用 VFS 核心挂载函数
	ret = do_mount(k_source, k_target, k_fstype, flags, k_data);

	// 释放内存资源
	if (k_data) kfree(k_data);
	kfree(k_fstype);
	kfree(k_target);
	if (k_source) kfree(k_source);

	return ret;
}




// User-facing open syscall
int64 sys_open(const char *pathname, int32 flags, mode_t mode) {
    if (!pathname) 
        return -EFAULT;
    
    // Copy pathname from user space
    char *kpathname = kmalloc(PATH_MAX);
    if (!kpathname)
        return -ENOMEM;
    
    if (copy_from_user(kpathname, pathname, PATH_MAX)) {
        kfree(kpathname);
        return -EFAULT;
    }
    
    // Call internal implementation
    int32 ret = do_open(kpathname, flags, mode);
    kfree(kpathname);
    return ret;
}

// User-facing close syscall
int64 sys_close(int32 fd) {
    return do_close(fd);
}

// User-facing lseek syscall
int64 sys_lseek(int32 fd, off_t offset, int32 whence) {
    return do_lseek(fd, offset, whence);
}

// User-facing read syscall
int64 sys_read(int32 fd, void *buf, size_t count) {
    if (!buf)
        return -EFAULT;
    
    // Allocate kernel buffer
    char *kbuf = kmalloc(count);
    if (!kbuf)
        return -ENOMEM;
    
    // Read into kernel buffer
    ssize_t ret = do_read(fd, kbuf, count);
    
    // Copy to user space if successful
    if (ret > 0) {
        if (copy_to_user(buf, kbuf, ret)) {
            kfree(kbuf);
            return -EFAULT;
        }
    }
    
    kfree(kbuf);
    return ret;
}

// User-facing write syscall
int64 sys_write(int32 fd, const void *buf, size_t count) {
    if (!buf)
        return -EFAULT;
    
    // Allocate kernel buffer
    char *kbuf = kmalloc(count);
    if (!kbuf)
        return -ENOMEM;
    
    // Copy from user space
    if (copy_from_user(kbuf, buf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }
    
    // Write from kernel buffer
    ssize_t ret = do_write(fd, kbuf, count);
    kfree(kbuf);
    return ret;
}