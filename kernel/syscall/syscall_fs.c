#include <kernel/fs/vfs/vfs.h>
#include <kernel/mm/uaccess.h>
#include <kernel/sched/process.h>
#include <kernel/syscall/syscall.h>
#include <kernel/types.h>
#include <spike_interface/spike_utils.h>
#include <util/string.h>

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
long sys_mount(const char* source_user, const char* target_user, const char* fstype_user, unsigned long flags, const void* data_user) {
	char* k_source = NULL;
	char* k_target = NULL;
	char* k_fstype = NULL;
	void* k_data = NULL;
	int ret = 0;

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
