在现代 Linux 实现中，address_space_ops 相关的工厂方法通常不直接暴露在 fstype 或 superblock 结构中，而是通过以下方式处理：

1. **文件系统特定的 inode 初始化函数**：每个文件系统实现通常在读取或创建 inode 时，会通过自己的 inode 初始化函数（如 `xxx_inode_init`）来设置适当的 address_space_ops。

2. **通过 inode_operations 中的方法**：文件系统会在 superblock 初始化时设置 inode_operations，然后通过这些操作间接关联 address_space_ops。

例如，在 ext4 文件系统中：

```c
// 在文件系统模块内定义静态常量
static const struct address_space_operations ext4_aops = { ... };
static const struct address_space_operations ext4_da_aops = { ... }; // 延迟分配版本

// 在 inode 初始化函数中根据文件属性设置合适的 aops
static void ext4_set_aops(struct inode *inode)
{
    if (ext4_should_use_delayed_allocation(inode))
        inode->i_mapping->a_ops = &ext4_da_aops;
    else
        inode->i_mapping->a_ops = &ext4_aops;
}

// 在读取 inode 时调用
static int ext4_read_inode(struct inode *inode)
{
    // ...初始化 inode 其他字段
    ext4_set_aops(inode);
    return 0;
}
```

所以，总结来说：

- 现代 Linux 既不在 fstype 也不在 superblock 中提供显式的 aops 工厂方法
- 而是在文件系统实现内部处理，通常在 inode 初始化或读取函数中设置
- 每个文件系统可以根据文件类型、特性或配置选择不同的 address_space_ops

这种方式更加灵活，允许文件系统基于多种因素（不仅仅是文件类型）来选择最适合的 address_space_ops，例如可以基于存储介质特性、挂载选项或文件特定属性。