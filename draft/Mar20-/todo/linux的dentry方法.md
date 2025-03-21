# Linux Dentry 核心方法参考指南

以下是 Linux 内核中 dentry 子系统的完整核心方法参考。这些函数构成了 Linux VFS 层中目录项操作的骨架。


## 引用计数管理


## 哈希表与查找

```c

/* 将dentry添加到哈希表 */
void d_rehash(struct dentry *entry);

/* 从哈希表中移除dentry */
void d_drop(struct dentry *dentry);

/* 在插入前检查名称哈希 */
struct dentry *d_hash_and_lookup(struct dentry *dir, struct qstr *name);
```

## LRU与缓存管理

```c

/* 收缩整个dentry缓存 */
void shrink_dcache_memory(int nr, gfp_t gfp_mask);

/* 收缩特定sb的dentry缓存 */
void shrink_dcache_sb(struct super_block *sb);

/* 清理指定inode的所有别名dentry */
void d_prune_aliases(struct inode *inode);

```

## 路径与名称操作

```c
/* 获取dentry的完整路径 */
char *__d_path(const struct path *path, char *buf, int buflen);
char *d_path(const struct path *path, char *buf, int buflen);

/* 路径名查找辅助函数 */
int path_lookup(const char *name, unsigned int flags, struct path *path);

/* 规范化路径 */
struct dentry *d_canonical_path(const struct path *path, struct path *actual_path);

/* 移动dentry到新位置 */
void d_move(struct dentry *dentry, struct dentry *target);

/* 设置dentry的操作函数表 */
void d_set_d_op(struct dentry *dentry, const struct dentry_operations *op);
```


## 特殊用途函数

```c
/* 处理已删除的dentry */
void d_genocide(struct dentry *root);

/* 将dentry标记为已过期 */
void d_invalidate(struct dentry *dentry);

/* 创建并处理别名dentry */
struct dentry *d_splice_alias(struct inode *inode, struct dentry *dentry);

/* 检查祖先关系 */
bool d_ancestor(struct dentry *p, struct dentry *child);

/* 处理正在被卸载的dentry */
void d_prune_aliases(struct inode *);

/* 使整个子树无效 */
void d_invalidate_recursive(struct dentry *dentry);
```

这个参考覆盖了 Linux 内核中 dentry 子系统几乎所有的核心功能。对于一个完整的 dentry 实现，您可以根据系统规模和需求选择实现其中的关键部分。