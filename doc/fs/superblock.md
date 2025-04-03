# superblock

## superblock的初始化流程

superblock 的初始化流程，从用户空间调用 mount() 开始，最终由 get_sb() 或 mount() 回调文件系统的填充函数（如 ext4_fill_super()），创建、填充 superblock 并建立根 dentry。
```c
用户空间：
  mount() 系统调用
    ↓
内核 VFS 层：
  sys_mount()                             // syscall 接口
    ↓
  do_mount()                              // 解析挂载点路径、文件系统类型等
    ↓
  vfs_kern_mount(fs_type, flags, dev, data) // 挂载主逻辑（✨调用 FS 虚函数）
    ↓
  fs_type->mount()                        // ✅ FS 类型特定的挂载方法（ext4_mount）
     或 legacy_get_sb() → fs_type->get_sb()
    ↓
  alloc_super()                           // 分配 super_block 结构
    ↓
  fill_super(sb, data)                    // ✅ FS 特定初始化（ext4_fill_super）
        ⤷ 设置 sb->s_blocksize、s_magic、s_op、s_fs_info ...
        ⤷ 创建 root inode → ext4_iget(sb, EXT4_ROOT_INO)
    ↓
  d_make_root(inode)                      // ✅ VFS 创建根 dentry
    ↓
  sb->s_root = root_dentry                // 根目录绑定
    ↓
  返回 struct super_block（挂载完成🎉）

```