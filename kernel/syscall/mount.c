#include <kernel/sched.h>
#include <kernel/vfs.h>
#include <kernel/syscall/syscall.h>
#include <kernel/mmu.h>
#include <kernel/util.h>


/**
 * do_mount - 挂载文件系统到指定挂载点
 * @source: 源设备名称
 * @target: 挂载点路径
 * @fstype_name: 文件系统类型
 * @flags: 挂载标志
 * @data: 文件系统特定数据
 *
 * 此函数是系统调用 sys_mount 的主要实现。它处理不同类型的挂载操作，
 * 包括常规挂载、bind挂载和重新挂载。
 *
 * 返回: 成功时返回0，失败时返回负的错误码
 */
int32 do_mount(const char* source, const char* target, const char* fstype_name, uint64 flags, const void* data) {
	struct path mount_path;
	struct fstype* type;
	struct vfsmount* newmnt;
	int32 ret;

	/* 查找文件系统类型 */
	type = fstype_lookup(fstype_name);
	if (!type) return -ENODEV;

	/* 查找挂载点路径 */
	ret = path_create(target, 0, &mount_path);
	if (ret) return ret;

	/* 处理特殊挂载标志 */
	if (flags & MS_BIND) {
		/* Bind mount 处理 */
		struct path source_path;
		ret = path_create(source, 0, &source_path);
		if (ret) goto out_path;

		newmnt = mount_bind(&source_path, flags);
		path_destroy(&source_path);
	} else if (flags & MS_REMOUNT) {
		/* 重新挂载处理 */
		ret = remount(&mount_path, flags, data);
		goto out_path;
	} else {
		/* 常规挂载 */

		newmnt = vfs_kern_mount(type, flags, source, data);
	}

	if (PTR_IS_ERROR(newmnt)) {
		ret = PTR_ERR(newmnt);
		goto out_path;
	}

	/* 添加新挂载点到挂载点层次结构 */
	ret = mount_add(newmnt, &mount_path, flags);
	if (ret) mount_unref(newmnt);

out_path:
	path_destroy(&mount_path);
	return ret;
}