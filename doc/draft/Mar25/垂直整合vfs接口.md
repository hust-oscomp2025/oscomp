# Linux VFS 接口与 vfs_kern_mount 并行表

下面是一个表格，展示了 Linux VFS 中主要的接口函数以及它们与你的 `vfs_kern_mount` 功能并行的函数，帮助你确认垂直整合情况。

| 功能分类 | 你的接口 | Linux VFS 接口 | 功能描述 | 垂直整合点 |
|---------|----------|--------------|----------|-----------|
| **挂载管理** | `vfs_kern_mount()` | `kern_mount()` | 在内核中挂载文件系统 | fstype, superblock |
|  | `superblock_acquireMount()` | `vfs_kern_mount()` | 创建挂载对象 | superblock, vfsmount |
|  | 尚未实现 | `do_mount()` | 处理挂载系统调用 | syscall → vfs |
| **卸载操作** | 尚未实现 | `do_umount()` | 卸载文件系统 | vfsmount, superblock |
|  | 尚未实现 | `mntput()` | 释放挂载点引用 | vfsmount |
| **文件系统注册** | `register_filesystem()` | `register_filesystem()` | 注册文件系统类型 | fstype |
|  | `unregister_filesystem()` | `unregister_filesystem()` | 注销文件系统类型 | fstype |
| **Superblock操作** | `fsType_acquireSuperblock()` | `sget()` | 获取或创建超级块 | fstype, superblock |
|  | `drop_super()` | `deactivate_super()` | 释放超级块 | superblock |
|  | `superblock_put()` | `put_super()` | 减少超级块引用计数 | superblock |
| **路径解析** | `path_lookup()` | `path_lookupat()` | 解析文件路径 | nameidata, dentry |
|  | 尚未实现 | `follow_mount()` | 处理路径中的挂载点 | vfsmount, dentry |
| **目录操作** | `vfs_mkdir()` | `vfs_mkdir()` | 创建目录 | inode, dentry |
|  | `vfs_rmdir()` | `vfs_rmdir()` | 删除目录 | inode, dentry |
| **文件操作** | `vfs_create()` | `vfs_create()` | 创建常规文件 | inode, dentry |
|  | `vfs_unlink()` | `vfs_unlink()` | 删除常规文件 | inode, dentry |
|  | `vfs_link()` | `vfs_link()` | 创建硬链接 | dentry, inode |
| **挂载点查找** | `lookup_mnt()` | `lookup_mnt()` | 查找挂载点 | vfsmount, path |
| **文件系统同步** | `sync_filesystem()` | `sync_filesystem()` | 同步文件系统数据 | superblock |

## 垂直整合分析

### 挂载流程整合

1. **系统调用层**
   - Linux: `sys_mount` → `do_mount`
   - 你的系统: 需要实现系统调用入口

2. **VFS挂载层**
   - Linux: `do_mount` → `do_new_mount` → `vfs_kern_mount`
   - 你的系统: `vfs_kern_mount` → `fsType_acquireSuperblock` + `superblock_acquireMount`

3. **文件系统特定层**
   - Linux: `type->mount` → `type->fs_type` + `fill_super`
   - 你的系统: 通过 `fsType` 结构体的操作函数

### 需要关注的关键点

1. **挂载命名空间支持**
   - Linux VFS 支持挂载命名空间隔离
   - 你的系统可能需要考虑是否实现这一特性

2. **路径解析与挂载点穿越**
   - Linux: `path_lookupat` → `follow_mount` → `follow_down_one`
   - 你的系统需要确保路径解析能正确处理挂载点

3. **文件系统特定挂载选项**
   - Linux: 每个文件系统有专门的 `mount` 和 `parse_options`
   - 你的系统通过 `data` 参数传递，需确保正确解析

4. **超级块复用**
   - Linux: 通过 `sget` 智能复用超级块
   - 你的系统通过 `fsType_acquireSuperblock` 实现类似功能

## 建议下一步实现

1. **完善挂载流程**
   - 实现 `do_mount` 系统调用接口
   - 实现 `follow_mount` 穿越挂载点功能

2. **卸载功能**
   - 实现 `do_umount` 和 `mntput` 接口
   - 完成卸载与引用计数管理

3. **文件系统特定挂载**
   - 为每种文件系统实现正确的 `mount` 和 `fill_super` 函数
   - 实现挂载选项解析功能

通过这些整合，你的 VFS 架构将与 Linux VFS 保持良好的兼容性，同时也能满足你的系统需求。