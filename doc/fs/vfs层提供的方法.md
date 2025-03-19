在 Linux 内核中，**VFS（Virtual File System）** 提供了一系列方法来抽象不同类型的文件系统。主要的接口包括：

---

## **1. `super_operations`（超级块操作）**
**管理文件系统的生命周期、挂载、卸载等**，每个具体文件系统都需要实现：
```c
struct super_operations {
    struct inode *(*alloc_inode)(struct super_block *sb);
    void (*destroy_inode)(struct inode *inode);
    void (*write_inode)(struct inode *inode, struct writeback_control *wbc);
    int (*sync_fs)(struct super_block *sb, int wait);
    int (*statfs)(struct dentry *dentry, struct kstatfs *buf);
    void (*put_super)(struct super_block *sb);
    int (*remount_fs)(struct super_block *sb, int *flags, char *data);
};
```
### **关键方法：**
- `alloc_inode()`：分配一个 inode 结构体。
- `destroy_inode()`：释放 inode 结构体。
- `write_inode()`：写回 inode 的元数据。
- `sync_fs()`：同步文件系统到磁盘。
- `statfs()`：返回文件系统统计信息（如 `df -h`）。
- `put_super()`：卸载文件系统时调用。

---

## **2. `inode_operations`（索引节点操作）**
**用于管理具体文件的元数据（如创建、删除、查找）**：
```c
struct inode_operations {
    struct dentry *(*lookup)(struct inode *dir, struct dentry *dentry, unsigned int flags);
    int (*create)(struct inode *dir, struct dentry *dentry, fmode_t mode, bool excl);
    int (*link)(struct dentry *old_dentry, struct inode *dir, struct dentry *new_dentry);
    int (*unlink)(struct inode *dir, struct dentry *dentry);
    int (*symlink)(struct inode *dir, struct dentry *dentry, const char *symname);
    int (*mkdir)(struct inode *dir, struct dentry *dentry, fmode_t mode);
    int (*rmdir)(struct inode *dir, struct dentry *dentry);
    int (*mknod)(struct inode *dir, struct dentry *dentry, fmode_t mode, dev_t rdev);
    int (*rename)(struct inode *old_dir, struct dentry *old_dentry,
                  struct inode *new_dir, struct dentry *new_dentry, unsigned int flags);
};
```
### **关键方法：**
- `lookup()`：查找文件，返回 `dentry`。
- `create()`：创建普通文件。
- `link()`：创建硬链接。
- `unlink()`：删除文件。
- `symlink()`：创建符号链接。
- `mkdir()`：创建目录。
- `rmdir()`：删除目录。
- `rename()`：重命名文件或目录。

---

## **3. `file_operations`（文件操作）**
**针对文件的读、写、打开、关闭等操作**：
```c
struct file_operations {
    loff_t (*llseek)(struct file *file, loff_t offset, int whence);
    ssize_t (*read)(struct file *file, char __user *buf, size_t count, loff_t *pos);
    ssize_t (*write)(struct file *file, const char __user *buf, size_t count, loff_t *pos);
    int (*open)(struct inode *inode, struct file *file);
    int (*release)(struct inode *inode, struct file *file);
    long (*unlocked_ioctl)(struct file *file, unsigned int cmd, unsigned long arg);
    int (*mmap)(struct file *file, struct vm_area_struct *vma);
    unsigned int (*poll)(struct file *file, struct poll_table_struct *wait);
};
```
### **关键方法：**
- `llseek()`：移动文件指针。
- `read()`：读取文件内容。
- `write()`：写入文件内容。
- `open()`：打开文件。
- `release()`：关闭文件。
- `unlocked_ioctl()`：特殊的 I/O 控制（如设备操作）。
- `mmap()`：文件映射到用户空间（`mmap()`）。

---

## **4. `dentry_operations`（目录项操作）**
**管理目录项（`dentry`），主要涉及缓存和重用**：
```c
struct dentry_operations {
    int (*d_revalidate)(struct dentry *dentry, unsigned int flags);
    void (*d_release)(struct dentry *dentry);
    void (*d_put_inode)(struct dentry *dentry, struct inode *inode);
    char *(*d_dname)(struct dentry *dentry, char *buffer, int buflen);
};
```
### **关键方法：**
- `d_revalidate()`：检查 `dentry` 是否仍然有效。
- `d_release()`：释放 `dentry`。
- `d_put_inode()`：释放 `dentry` 关联的 `inode`。
- `d_dname()`：生成 `dentry` 的路径名称。

---

## **5. `address_space_operations`（内存映射操作）**
**管理页缓存和写回机制**：
```c
struct address_space_operations {
    int (*writepage)(struct page *page, struct writeback_control *wbc);
    int (*readpage)(struct file *file, struct page *page);
    int (*write_begin)(struct file *file, struct address_space *mapping,
                       loff_t pos, unsigned len, unsigned flags, struct page **pagep, void **fsdata);
    int (*write_end)(struct file *file, struct address_space *mapping,
                     loff_t pos, unsigned len, unsigned copied, struct page *page, void *fsdata);
};
```
### **关键方法：**
- `writepage()`：将内存页写入磁盘。
- `readpage()`：从磁盘读取页到缓存。
- `write_begin()` / `write_end()`：管理写入时的页缓存。

---

## **6. `file_system_type`（文件系统类型）**
**用于注册新的文件系统类型**：
```c
struct file_system_type {
    const char *name;
    struct dentry *(*mount) (struct file_system_type *, int,
                             const char *, void *);
    void (*kill_sb) (struct super_block *);
};
```
### **关键方法：**
- `mount()`：挂载文件系统。
- `kill_sb()`：卸载文件系统。

---

## **总结**
Linux VFS 主要提供以下 **六大接口**，每个具体文件系统都要实现它们：
1. **`super_operations`**（管理超级块）
2. **`inode_operations`**（管理文件元数据）
3. **`file_operations`**（管理文件读写）
4. **`dentry_operations`**（管理目录项）
5. **`address_space_operations`**（管理页缓存）
6. **`file_system_type`**（管理文件系统类型）

如果你想在 RISC-V 竞赛内核中实现 VFS，建议先从 `file_operations` 和 `inode_operations` 开始，毕竟基本的 `open/read/write` 先跑起来，再扩展 `dentry_operations` 进行优化。你准备怎么设计它？