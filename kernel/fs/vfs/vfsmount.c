#include <kernel/fs/vfs/vfs.h>
#include <kernel/types.h>


/**
 * do_mount - 挂载文件系统到指定挂载点
 * @source: 源设备名称
 * @target: 挂载点路径
 * @fstype: 文件系统类型
 * @flags: 挂载标志
 * @data: 文件系统特定数据
 *
 * 此函数是系统调用 sys_mount 的主要实现。它处理不同类型的挂载操作，
 * 包括常规挂载、bind挂载和重新挂载。
 *
 * 返回: 成功时返回0，失败时返回负的错误码
 */
int do_mount(const char* source, const char* target, const char* fstype, unsigned long flags, void* data) {
	struct path mountpoint;
	struct fstype* type;
	struct vfsmount* newmnt;
	int ret;

	/* 查找文件系统类型 */
	type = get_fstype(fstype);
	if (!type) return -ENODEV;

	/* 查找挂载点路径 */
	ret = path_lookup(target, 0, &mountpoint);
	if (ret) return ret;

	/* 处理特殊挂载标志 */
	if (flags & MS_BIND) {
		/* Bind mount 处理 */
		struct path source_path;
		ret = path_lookup(source, 0, &source_path);
		if (ret) goto out_path;

		newmnt = do_bind_mount(&source_path, flags);
		path_put(&source_path);
	} else if (flags & MS_REMOUNT) {
		/* 重新挂载处理 */
		ret = do_remount(&mountpoint, flags, data);
		goto out_path;
	} else {
		/* 常规挂载 */
		newmnt = do_new_mount(type, flags, source, data);
	}

	if (IS_ERR(newmnt)) {
		ret = PTR_ERR(newmnt);
		goto out_path;
	}

	/* 添加新挂载点到挂载点层次结构 */
	ret = do_add_mount(newmnt, &mountpoint, flags);
	if (ret) put_mount(newmnt);

out_path:
	path_put(&mountpoint);
	return ret;
}
