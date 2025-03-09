/*
 * RamFS - 内存文件系统
 * 一个简单的基于内存和页缓存的文件系统实现
 */

 #include <kernel/mm/kmalloc.h>
 #include <kernel/mm/page.h>
 #include <util/string.h>
 #include <kernel/fs/file.h>
 #include <kernel/fs/inode.h>
 #include <kernel/fs/vfs.h>
 #include <kernel/fs/ramfs.h>

 #include <kernel/fs/super_block.h>
 #include <kernel/fs/address_space.h>
 #include <kernel/types.h>
 #include <kernel/mm/mmap.h>
 #include <kernel/mm/mm_struct.h>
 #include <spike_interface/spike_utils.h>
 
 /**** 内部数据结构定义 ****/
 
 /* 目录项结构 */
 struct ramfs_direntry {
		 uint32 inum;                    /* inode编号 */
		 char name[MAX_FILE_NAME_LEN];   /* 文件名 */
 };
 
 /* 内存inode结构 */
 struct ramfs_inode {
		 uint32 inum;         /* inode编号 */
		 uint32 type;         /* 文件类型 (RAMFS_FILE/RAMFS_DIR) */
		 uint32 nlinks;       /* 硬链接数 */
		 uint64 size;         /* 文件大小 */
		 uint64 uid, gid;     /* 用户ID和组ID */
		 uint64 atime, mtime, ctime; /* 访问、修改、创建时间 */
 };
 
 /* 文件系统特定的inode数据 */
 struct ramfs_inode_info {
		 struct ramfs_inode *mem_inode;  /* 指向内存inode */
		 void *dir_data;                 /* 目录数据（若为目录） */
 };
 
 /* 文件系统特定的超级块数据 */
 struct ramfs_sb_info {
		 uint32 next_ino;     /* 下一个可用的inode编号 */
		 uint32 inode_count;  /* inode计数 */
		 struct inode *root_inode;  /* 根目录inode */
 };
 
 /**** 前向声明 ****/
 
 /* 地址空间操作 */
 static int ramfs_writepage(struct page *page, void *wbc);
 static int ramfs_readpage(struct address_space *mapping, struct page *page);
 static int ramfs_writepages(struct address_space *mapping, void *wbc);
 static int ramfs_readpages(struct address_space *mapping, struct list_head *pages, unsigned nr_pages);
 static int ramfs_set_page_dirty(struct page *page);
 static int ramfs_releasepage(struct page *page);
 static void ramfs_invalidatepage(struct page *page, unsigned int offset, unsigned int length);
 
 /* 文件操作 */
 static int ramfs_file_open(struct inode *inode, struct file *file);
 static int ramfs_file_release(struct inode *inode, struct file *file);
 static ssize_t ramfs_file_read(struct file *file, uaddr buf, size_t count, loff_t *pos);
 static ssize_t ramfs_file_write(struct file *file, uaddr buf, size_t count, loff_t *pos);
 static loff_t ramfs_file_llseek(struct file *file, loff_t offset, int whence);
 
 /* 目录操作 */
 static int ramfs_dir_open(struct inode *inode, struct file *file);
 static int ramfs_dir_release(struct inode *inode, struct file *file);
 static ssize_t ramfs_dir_read(struct file *file, uaddr buf, size_t count, loff_t *pos);
 
 /* inode操作 */
 static struct inode *ramfs_create(struct inode *dir, struct dentry *dentry);
 static struct inode *ramfs_lookup(struct inode *dir, struct dentry *dentry);
 static struct inode *ramfs_mkdir(struct inode *dir, struct dentry *dentry);
 static int ramfs_link(struct inode *dir, struct dentry *dentry, struct inode *inode);
 static int ramfs_unlink(struct inode *dir, struct dentry *dentry, struct inode *inode);
 static int ramfs_write_back_inode(struct inode *inode);
 static int ramfs_readdir(struct inode *dir, struct dir *dir_out, int *offset);
 
 /**** 操作函数结构定义 ****/
 
 /* 地址空间操作表 */
 static const struct address_space_operations ramfs_aops = {
		 .writepage = ramfs_writepage,
		 .readpage = ramfs_readpage,
		 .writepages = ramfs_writepages,
		 .readpages = ramfs_readpages,
		 .set_page_dirty = ramfs_set_page_dirty,
		 .releasepage = ramfs_releasepage,
		 .invalidatepage = ramfs_invalidatepage
 };
 
 /* 文件操作表 */
 static const struct file_operations ramfs_file_operations = {
		 .open = ramfs_file_open,
		 .release = ramfs_file_release,
		 .read = ramfs_file_read,
		 .write = ramfs_file_write,
		 .llseek = ramfs_file_llseek
 };
 
 /* 目录操作表 */
 static const struct file_operations ramfs_dir_operations = {
		 .open = ramfs_dir_open,
		 .release = ramfs_dir_release,
		 .read = ramfs_dir_read
 };
 
 /* 文件inode操作表 */
 static const struct inode_operations ramfs_file_inode_operations = {
		 .viop_write_back_vinode = ramfs_write_back_inode
 };
 
 /* 目录inode操作表 */
 static const struct inode_operations ramfs_dir_inode_operations = {
		 .viop_create = ramfs_create,
		 .viop_lookup = ramfs_lookup,
		 .viop_mkdir = ramfs_mkdir,
		 .viop_link = ramfs_link,
		 .viop_unlink = ramfs_unlink,
		 .viop_write_back_vinode = ramfs_write_back_inode,
		 .viop_readdir = ramfs_readdir
 };
 
 /**** 文件系统初始化与注册 ****/
 
 /* 注册文件系统类型 */
 int register_ramfs(void) {
		 struct file_system_type *fs_type = kmalloc(sizeof(struct file_system_type));
		 if (!fs_type)
				 return -1;
		 
		 fs_type->type_num = RAMFS_TYPE;
		 fs_type->get_superblock = ramfs_get_sb;
 
		 for (int i = 0; i < MAX_SUPPORTED_FS; i++) {
				 if (fs_list[i] == NULL) {
						 fs_list[i] = fs_type;
						 sprint("register_ramfs: registered successfully.\n");
						 return 0;
				 }
		 }
		 
		 kfree(fs_type);
		 return -1;
 }
 
 /* 初始化内存文件系统设备 */
 struct device *init_ramfs_device(char *name) {
		 // 查找RamFS在已注册文件系统列表中
		 struct file_system_type *fs_type = NULL;
		 for (int i = 0; i < MAX_SUPPORTED_FS; i++) {
				 if (fs_list[i] != NULL && fs_list[i]->type_num == RAMFS_TYPE) {
						 fs_type = fs_list[i];
						 break;
				 }
		 }
		 
		 if (!fs_type) {
				 panic("init_ramfs_device: No RamFS file system found!\n");
		 }
		 
		 // 分配VFS设备
		 struct device *device = kmalloc(sizeof(struct device));
		 if (!device) {
				 panic("init_ramfs_device: Failed to allocate device!\n");
		 }
		 
		 // 设置设备名称和ID
		 strcpy(device->dev_name, name);
		 device->dev_id = 0;  // 简化，只支持一个RamFS实例
		 device->fs_type = fs_type;
		 
		 // 添加到VFS设备列表
		 for (int i = 0; i < MAX_VFS_DEV; i++) {
				 if (vfs_dev_list[i] == NULL) {
						 vfs_dev_list[i] = device;
						 break;
				 }
		 }
		 
		 sprint("init_ramfs_device: Initialized device %s\n", name);
		 return device;
 }
 
 /* 格式化内存文件系统 */
 int ramfs_format_dev(struct device *dev) {
		 // 该函数在内存文件系统中主要是初始化超级块信息
		 // 实际格式化操作会在超级块获取函数中完成
		 sprint("ramfs_format_dev: Formatting %s\n", dev->dev_name);
		 return 0;
 }
 
 /**** 超级块操作 ****/
 
 /* 获取超级块 */
 struct super_block *ramfs_get_sb(struct device *dev) {
		 // 创建超级块
		 struct super_block *sb = kmalloc(sizeof(struct super_block));
		 if (!sb) {
				 panic("ramfs_get_sb: Failed to allocate superblock!\n");
		 }
		 
		 // 初始化超级块
		 sb->magic = RAMFS_MAGIC;
		 sb->size = 0;  // 内存文件系统不关心大小
		 sb->nblocks = 0;
		 sb->ninodes = 0;
		 sb->s_dev = dev;
		 
		 // 创建超级块私有信息
		 struct ramfs_sb_info *sbi = kmalloc(sizeof(struct ramfs_sb_info));
		 if (!sbi) {
				 kfree(sb);
				 panic("ramfs_get_sb: Failed to allocate superblock info!\n");
		 }
		 
		 // 初始化超级块信息
		 sbi->next_ino = 1;  // 0号预留给根目录
		 sbi->inode_count = 0;
		 sbi->root_inode = NULL;
		 sb->s_fs_info = sbi;
		 
		 // 创建根目录inode
		 struct ramfs_inode *root_mem_inode = kmalloc(sizeof(struct ramfs_inode));
		 if (!root_mem_inode) {
				 kfree(sbi);
				 kfree(sb);
				 panic("ramfs_get_sb: Failed to allocate root inode!\n");
		 }
		 
		 // 初始化根目录内存inode
		 root_mem_inode->inum = 0;
		 root_mem_inode->type = RAMFS_DIR;
		 root_mem_inode->nlinks = 1;
		 root_mem_inode->size = 0;
		 root_mem_inode->uid = root_mem_inode->gid = 0;
		 root_mem_inode->atime = root_mem_inode->mtime = root_mem_inode->ctime = 0;
		 
		 // 创建根目录VFS inode
		 struct inode *root_inode = default_alloc_vinode(sb);
		 if (!root_inode) {
				 kfree(root_mem_inode);
				 kfree(sbi);
				 kfree(sb);
				 panic("ramfs_get_sb: Failed to allocate root VFS inode!\n");
		 }
		 
		 // 初始化根目录VFS inode
		 root_inode->i_ino = 0;
		 root_inode->i_mode = S_IFDIR;
		 root_inode->i_nlink = 1;
		 root_inode->i_size = 0;
		 root_inode->blocks = 0;
		 root_inode->i_op = &ramfs_dir_inode_operations;
		 root_inode->i_fop = &ramfs_dir_operations;
		 
		 // 创建根目录的地址空间
		 root_inode->i_mapping = address_space_create(root_inode, &ramfs_aops);
		 if (!root_inode->i_mapping) {
				 kfree(root_inode);
				 kfree(root_mem_inode);
				 kfree(sbi);
				 kfree(sb);
				 panic("ramfs_get_sb: Failed to allocate address space!\n");
		 }
		 
		 // 创建inode信息
		 struct ramfs_inode_info *inode_info = kmalloc(sizeof(struct ramfs_inode_info));
		 if (!inode_info) {
				 address_space_destroy(root_inode->i_mapping);
				 kfree(root_inode);
				 kfree(root_mem_inode);
				 kfree(sbi);
				 kfree(sb);
				 panic("ramfs_get_sb: Failed to allocate inode info!\n");
		 }
		 
		 // 初始化inode信息
		 inode_info->mem_inode = root_mem_inode;
		 inode_info->dir_data = NULL;  // 初始为空目录
		 root_inode->i_private = inode_info;
		 
		 // 创建根目录的dentry
		 struct dentry *root_dentry = alloc_vfs_dentry("/", root_inode, NULL);
		 if (!root_dentry) {
				 kfree(inode_info);
				 address_space_destroy(root_inode->i_mapping);
				 kfree(root_inode);
				 kfree(root_mem_inode);
				 kfree(sbi);
				 kfree(sb);
				 panic("ramfs_get_sb: Failed to allocate root dentry!\n");
		 }
		 
		 sb->s_root = root_dentry;
		 sbi->root_inode = root_inode;
		 sbi->inode_count++;
		 
		 sprint("ramfs_get_sb: RamFS superblock initialized successfully\n");
		 return sb;
 }
 
 /**** 地址空间操作实现 ****/
 
 /* 将页写回存储 */
 static int ramfs_writepage(struct page *page, void *wbc) {
		 /* 在内存文件系统中，我们不需要真正写回存储 */
		 /* 只需清除页的脏标志 */
		 clear_page_dirty(page);
		 return 0;
 }
 
 /* 读取页内容 */
 static int ramfs_readpage(struct address_space *mapping, struct page *page) {
		 /* 在内存文件系统中，页面可能是首次访问，需要分配并初始化 */
		 if (!page->virtual_address) {
				 page->virtual_address = alloc_page_buffer();
				 if (!page->virtual_address)
						 return -1;
				 
				 /* 新页面初始化为0 */
				 memset(page->virtual_address, 0, PAGE_SIZE);
		 }
		 
		 /* 标记页为最新 */
		 set_page_uptodate(page);
		 return 0;
 }
 
 /* 写回多个页 */
 static int ramfs_writepages(struct address_space *mapping, void *wbc) {
		 /* 简化版：我们可以直接返回成功，因为是内存文件系统 */
		 return 0;
 }
 
 /* 读取多个页 */
 static int ramfs_readpages(struct address_space *mapping, struct list_head *pages, unsigned nr_pages) {
		 /* 简化版：逐个读取页 */
		 struct page *page;
		 list_for_each_entry(page, pages, lru) {
				 ramfs_readpage(mapping, page);
		 }
		 return 0;
 }
 
 /* 设置页为脏 */
 static int ramfs_set_page_dirty(struct page *page) {
		 set_page_dirty(page);
		 return 0;
 }
 
 /* 释放页 */
 static int ramfs_releasepage(struct page *page) {
		 /* 如果页是脏的，不能释放 */
		 if (test_page_dirty(page))
				 return 0;
		 
		 /* 释放页 */
		 if (page->virtual_address) {
				 free_page_buffer(page->virtual_address);
				 page->virtual_address = NULL;
		 }
		 return 1;
 }
 
 /* 使页无效 */
 static void ramfs_invalidatepage(struct page *page, unsigned int offset, unsigned int length) {
		 /* 在内存文件系统中，我们可以简单地将页标记为非脏 */
		 clear_page_dirty(page);
 }
 
 /**** 文件操作实现 ****/
 
 /* 打开文件 */
 static int ramfs_file_open(struct inode *inode, struct file *file) {
		 file->f_op = &ramfs_file_operations;
		 return 0;
 }
 
 /* 关闭文件 */
 static int ramfs_file_release(struct inode *inode, struct file *file) {
		 return 0;
 }
 
 /* 读取文件 */
 static ssize_t ramfs_file_read(struct file *file, uaddr buf, size_t count, loff_t *pos) {
		 struct inode *inode = file->f_dentry->dentry_inode;
		 struct address_space *mapping = inode->i_mapping;
		 ssize_t read_bytes = 0;
		 
		 /* 检查读取是否超出文件尾 */
		 if (*pos >= inode->i_size)
				 return 0;
		 
		 /* 调整读取长度不超过文件末尾 */
		 if (*pos + count > inode->i_size)
				 count = inode->i_size - *pos;
		 
		 /* 逐页读取 */
		 while (count > 0) {
				 /* 计算当前页索引和页内偏移 */
				 uint64 page_index = *pos / PAGE_SIZE;
				 uint64 page_offset = *pos % PAGE_SIZE;
				 uint64 bytes_this_page = PAGE_SIZE - page_offset;
				 
				 if (bytes_this_page > count)
						 bytes_this_page = count;
				 
				 /* 获取页 */
				 struct page *page = find_or_create_page(mapping, page_index);
				 if (!page)
						 return read_bytes ? read_bytes : -1;
				 
				 /* 确保页内容是最新的 */
				 if (!page_uptodate(page)) {
						 if (ramfs_readpage(mapping, page) != 0) {
								 put_page(page);
								 return read_bytes ? read_bytes : -1;
						 }
				 }
				 
				 /* 复制数据到用户缓冲区 */
				 copy_from_page(page, (char *)buf + read_bytes, bytes_this_page, page_offset);
				 
				 /* 减少页引用计数 */
				 put_page(page);
				 
				 /* 更新位置和计数 */
				 *pos += bytes_this_page;
				 count -= bytes_this_page;
				 read_bytes += bytes_this_page;
		 }
		 
		 return read_bytes;
 }
 
 /* 写入文件 */
 static ssize_t ramfs_file_write(struct file *file, uaddr buf, size_t count, loff_t *pos) {
		 struct inode *inode = file->f_dentry->dentry_inode;
		 struct address_space *mapping = inode->i_mapping;
		 ssize_t written_bytes = 0;
		 
		 /* 逐页写入 */
		 while (count > 0) {
				 /* 计算当前页索引和页内偏移 */
				 uint64 page_index = *pos / PAGE_SIZE;
				 uint64 page_offset = *pos % PAGE_SIZE;
				 uint64 bytes_this_page = PAGE_SIZE - page_offset;
				 
				 if (bytes_this_page > count)
						 bytes_this_page = count;
				 
				 /* 获取页 */
				 struct page *page = find_or_create_page(mapping, page_index);
				 if (!page)
						 return written_bytes ? written_bytes : -1;
				 
				 /* 确保页内容是最新的 */
				 if (!page_uptodate(page)) {
						 if (ramfs_readpage(mapping, page) != 0) {
								 put_page(page);
								 return written_bytes ? written_bytes : -1;
						 }
				 }
				 
				 /* 复制数据到页 */
				 copy_to_page(page, (const char *)buf + written_bytes, bytes_this_page, page_offset);
				 
				 /* 减少页引用计数 */
				 put_page(page);
				 
				 /* 更新位置和计数 */
				 *pos += bytes_this_page;
				 count -= bytes_this_page;
				 written_bytes += bytes_this_page;
		 }
		 
		 /* 更新文件大小（如果写入位置超过了当前文件大小） */
		 if (*pos > inode->i_size) {
				 inode->i_size = *pos;
				 /* 标记inode为脏 */
				 ramfs_write_back_inode(inode);
		 }
		 
		 return written_bytes;
 }
 
 /* 设置文件偏移 */
 static loff_t ramfs_file_llseek(struct file *file, loff_t offset, int whence) {
		 struct inode *inode = file->f_dentry->dentry_inode;
		 loff_t new_pos;
		 
		 switch (whence) {
				 case SEEK_SET:
						 new_pos = offset;
						 break;
				 case SEEK_CUR:
						 new_pos = file->f_pos + offset;
						 break;
				 case SEEK_END:
						 new_pos = inode->i_size + offset;
						 break;
				 default:
						 return -1;
		 }
		 
		 /* 检查是否合法 */
		 if (new_pos < 0)
				 return -1;
		 
		 file->f_pos = new_pos;
		 return new_pos;
 }
 
 /**** 目录操作实现 ****/
 
 /* 打开目录 */
 static int ramfs_dir_open(struct inode *inode, struct file *file) {
		 file->f_op = &ramfs_dir_operations;
		 file->f_pos = 0;
		 return 0;
 }
 
 /* 关闭目录 */
 static int ramfs_dir_release(struct inode *inode, struct file *file) {
		 return 0;
 }
 
 /* 读取目录 - 目录操作通常使用readdir，但我们提供读取接口以便兼容 */
 static ssize_t ramfs_dir_read(struct file *file, uaddr buf, size_t count, loff_t *pos) {
		 /* 不支持直接读取目录内容 */
		 return -1;
 }
 
 /**** inode操作实现 ****/
 
 /* 创建新文件 */
 static struct inode *ramfs_create(struct inode *dir, struct dentry *dentry) {
		 struct ramfs_sb_info *sbi = (struct ramfs_sb_info *)dir->sb->s_fs_info;
		 
		 /* 分配新的inode编号 */
		 uint32 new_ino = sbi->next_ino++;
		 
		 /* 创建内存inode */
		 struct ramfs_inode *mem_inode = kmalloc(sizeof(struct ramfs_inode));
		 if (!mem_inode)
				 return NULL;
		 
		 /* 初始化内存inode */
		 mem_inode->inum = new_ino;
		 mem_inode->type = RAMFS_FILE;
		 mem_inode->nlinks = 1;
		 mem_inode->size = 0;
		 mem_inode->uid = mem_inode->gid = 0;
		 mem_inode->atime = mem_inode->mtime = mem_inode->ctime = 0; /* 应该使用当前时间 */
		 
		 /* 创建VFS inode */
		 struct inode *inode = default_alloc_vinode(dir->sb);
		 if (!inode) {
				 kfree(mem_inode);
				 return NULL;
		 }
		 
		 /* 初始化VFS inode */
		 inode->i_ino = new_ino;
		 inode->i_mode = S_IFREG;
		 inode->i_nlink = 1;
		 inode->i_size = 0;
		 inode->blocks = 0;
		 inode->i_op = &ramfs_file_inode_operations;
		 inode->i_fop = &ramfs_file_operations;
		 
		 /* 创建地址空间 */
		 inode->i_mapping = address_space_create(inode, &ramfs_aops);
		 if (!inode->i_mapping) {
				 kfree(mem_inode);
				 kfree(inode);
				 return NULL;
		 }
		 
		 /* 存储内存inode */
		 struct ramfs_inode_info *inode_info = kmalloc(sizeof(struct ramfs_inode_info));
		 if (!inode_info) {
				 address_space_destroy(inode->i_mapping);
				 kfree(mem_inode);
				 kfree(inode);
				 return NULL;
		 }
		 
		 inode_info->mem_inode = mem_inode;
		 inode_info->dir_data = NULL;
		 inode->i_private = inode_info;
		 
		 /* 增加inode计数 */
		 sbi->inode_count++;
		 
		 /* 添加到目录 */
		 struct ramfs_inode_info *dir_info = dir->i_private;
		 struct ramfs_direntry *entries = dir_info->dir_data;
		 uint64 dir_size = dir->i_size;
		 uint64 entry_count = dir_size / sizeof(struct ramfs_direntry);
		 
		 /* 重新分配目录条目数组 */
		 entries = krealloc(entries, dir_size + sizeof(struct ramfs_direntry));
		 if (!entries) {
				 address_space_destroy(inode->i_mapping);
				 kfree(mem_inode);
				 kfree(inode_info);
				 kfree(inode);
				 return NULL;
		 }
		 
		 /* 添加新条目 */
		 struct ramfs_direntry *new_entry = &entries[entry_count];
		 new_entry->inum = new_ino;
		 strncpy(new_entry->name, dentry->name, MAX_FILE_NAME_LEN - 1);
		 new_entry->name[MAX_FILE_NAME_LEN - 1] = '\0';
		 
		 /* 更新目录信息 */
		 dir_info->dir_data = entries;
		 dir->i_size += sizeof(struct ramfs_direntry);
		 
		 return inode;
 }
 
 /* 查找目录中的inode */
 static struct inode *ramfs_lookup(struct inode *dir, struct dentry *dentry) {
		 struct ramfs_inode_info *dir_info = dir->i_private;
		 struct ramfs_direntry *entries = dir_info->dir_data;
		 uint64 entry_count = dir->i_size / sizeof(struct ramfs_direntry);
		 
		 /* 查找名称匹配的条目 */
		 for (uint64 i = 0; i < entry_count; i++) {
				 if (strcmp(entries[i].name, dentry->name) == 0) {
						 uint32 inum = entries[i].inum;
						 
						 /* 创建并返回inode */
						 struct inode *inode = default_alloc_vinode(dir->sb);
						 if (!inode)
								 return NULL;
						 
						 /* 设置inode编号 */
						 inode->i_ino = inum;
						 
						 /* 创建一个新的内存inode结构 */
						 struct ramfs_inode *mem_inode = kmalloc(sizeof(struct ramfs_inode));
						 if (!mem_inode) {
								 kfree(inode);
								 return NULL;
						 }
						 
						 /* 初始化内存inode（简化版）*/
						 mem_inode->inum = inum;
						 
						 /* 根据inode类型决定操作表 */
						 /* 注意：在实际实现中应该从磁盘或缓存读取inode信息 */
						 /* 这里我们假设所有inode都已在内存中，且type通过查询可得 */
						 if (inum == 0) { // 假设0是根目录
								 mem_inode->type = RAMFS_DIR;
								 inode->i_mode = S_IFDIR;
								 inode->i_op = &ramfs_dir_inode_operations;
								 inode->i_fop = &ramfs_dir_operations;
						 } else {
								 /* 这里我们简单假设非0的inode都是文件 */
								 mem_inode->type = RAMFS_FILE;
								 inode->i_mode = S_IFREG;
								 inode->i_op = &ramfs_file_inode_operations;
								 inode->i_fop = &ramfs_file_operations;
						 }
						 
						 /* 创建inode信息 */
						 struct ramfs_inode_info *inode_info = kmalloc(sizeof(struct ramfs_inode_info));
						 if (!inode_info) {
								 kfree(mem_inode);
								 kfree(inode);
								 return NULL;
						 }
						 
						 /* 初始化inode信息 */
						 inode_info->mem_inode = mem_inode;
						 inode_info->dir_data = NULL; /* 如果是目录，稍后会加载 */
						 inode->i_private = inode_info;
						 
						 /* 创建地址空间 */
						 inode->i_mapping = address_space_create(inode, &ramfs_aops);
						 if (!inode->i_mapping) {
								 kfree(inode_info);
								 kfree(mem_inode);
								 kfree(inode);
								 return NULL;
						 }
						 
						 return inode;
				 }
		 }
		 
		 /* 未找到 */
		 return NULL;
 }

 /* 创建新目录 */
static struct inode *ramfs_mkdir(struct inode *dir, struct dentry *dentry) {
	struct ramfs_sb_info *sbi = (struct ramfs_sb_info *)dir->sb->s_fs_info;
	
	/* 分配新的inode编号 */
	uint32 new_ino = sbi->next_ino++;
	
	/* 创建内存inode */
	struct ramfs_inode *mem_inode = kmalloc(sizeof(struct ramfs_inode));
	if (!mem_inode)
			return NULL;
	
	/* 初始化内存inode */
	mem_inode->inum = new_ino;
	mem_inode->type = RAMFS_DIR;
	mem_inode->nlinks = 2;  // 目录有自己的"."和父目录的".."链接
	mem_inode->size = 0;
	mem_inode->uid = mem_inode->gid = 0;
	mem_inode->atime = mem_inode->mtime = mem_inode->ctime = 0; // 应该使用当前时间
	
	/* 创建VFS inode */
	struct inode *inode = default_alloc_vinode(dir->sb);
	if (!inode) {
			kfree(mem_inode);
			return NULL;
	}
	
	/* 初始化VFS inode */
	inode->i_ino = new_ino;
	inode->i_mode = S_IFDIR;
	inode->i_nlink = 2;
	inode->i_size = 0;
	inode->blocks = 0;
	inode->i_op = &ramfs_dir_inode_operations;
	inode->i_fop = &ramfs_dir_operations;
	
	/* 创建地址空间 */
	inode->i_mapping = address_space_create(inode, &ramfs_aops);
	if (!inode->i_mapping) {
			kfree(mem_inode);
			kfree(inode);
			return NULL;
	}
	
	/* 创建inode信息 */
	struct ramfs_inode_info *inode_info = kmalloc(sizeof(struct ramfs_inode_info));
	if (!inode_info) {
			address_space_destroy(inode->i_mapping);
			kfree(mem_inode);
			kfree(inode);
			return NULL;
	}
	
	/* 初始化inode信息 */
	inode_info->mem_inode = mem_inode;
	inode_info->dir_data = NULL;  // 空目录
	inode->i_private = inode_info;
	
	/* 增加inode计数 */
	sbi->inode_count++;
	
	/* 添加到父目录 */
	struct ramfs_inode_info *dir_info = dir->i_private;
	struct ramfs_direntry *entries = dir_info->dir_data;
	uint64 dir_size = dir->i_size;
	uint64 entry_count = dir_size / sizeof(struct ramfs_direntry);
	
	/* 重新分配目录条目数组 */
	entries = krealloc(entries, dir_size + sizeof(struct ramfs_direntry));
	if (!entries) {
			address_space_destroy(inode->i_mapping);
			kfree(mem_inode);
			kfree(inode_info);
			kfree(inode);
			return NULL;
	}
	
	/* 添加新条目 */
	struct ramfs_direntry *new_entry = &entries[entry_count];
	new_entry->inum = new_ino;
	strncpy(new_entry->name, dentry->name, MAX_FILE_NAME_LEN - 1);
	new_entry->name[MAX_FILE_NAME_LEN - 1] = '\0';
	
	/* 更新目录信息 */
	dir_info->dir_data = entries;
	dir->i_size += sizeof(struct ramfs_direntry);
	
	/* 增加父目录的链接计数 */
	dir->i_nlink++;
	
	return inode;
}

/* 创建硬链接 */
static int ramfs_link(struct inode *dir, struct dentry *dentry, struct inode *inode) {
	struct ramfs_inode_info *dir_info = dir->i_private;
	struct ramfs_direntry *entries = dir_info->dir_data;
	uint64 dir_size = dir->i_size;
	uint64 entry_count = dir_size / sizeof(struct ramfs_direntry);
	
	/* 检查链接目标是否为目录 */
	if (S_ISDIR(inode->i_mode)) {
			return -1;  // 不允许为目录创建硬链接
	}
	
	/* 重新分配目录条目数组 */
	entries = krealloc(entries, dir_size + sizeof(struct ramfs_direntry));
	if (!entries) {
			return -1;
	}
	
	/* 添加新条目 */
	struct ramfs_direntry *new_entry = &entries[entry_count];
	new_entry->inum = inode->i_ino;
	strncpy(new_entry->name, dentry->name, MAX_FILE_NAME_LEN - 1);
	new_entry->name[MAX_FILE_NAME_LEN - 1] = '\0';
	
	/* 更新目录信息 */
	dir_info->dir_data = entries;
	dir->i_size += sizeof(struct ramfs_direntry);
	
	/* 增加inode的链接计数 */
	inode->i_nlink++;
	struct ramfs_inode_info *inode_info = inode->i_private;
	inode_info->mem_inode->nlinks++;
	
	return 0;
}

/* 删除链接 */
static int ramfs_unlink(struct inode *dir, struct dentry *dentry, struct inode *inode) {
	struct ramfs_inode_info *dir_info = dir->i_private;
	struct ramfs_direntry *entries = dir_info->dir_data;
	uint64 dir_size = dir->i_size;
	uint64 entry_count = dir_size / sizeof(struct ramfs_direntry);
	int found = 0;
	
	/* 查找要删除的条目 */
	for (uint64 i = 0; i < entry_count; i++) {
			if (strcmp(entries[i].name, dentry->name) == 0) {
					/* 移动后面的条目来填补空缺 */
					if (i < entry_count - 1) {
							memmove(&entries[i], &entries[i + 1], 
											(entry_count - i - 1) * sizeof(struct ramfs_direntry));
					}
					found = 1;
					break;
			}
	}
	
	if (!found) {
			return -1;
	}
	
	/* 更新目录大小 */
	dir->i_size -= sizeof(struct ramfs_direntry);
	
	/* 如果是目录，减少父目录的链接计数 */
	if (S_ISDIR(inode->i_mode)) {
			dir->i_nlink--;
	}
	
	/* 减少inode的链接计数 */
	inode->i_nlink--;
	struct ramfs_inode_info *inode_info = inode->i_private;
	inode_info->mem_inode->nlinks--;
	
	/* 如果链接计数为0，可以考虑释放inode资源 */
	if (inode->i_nlink == 0) {
			/* 在完整实现中，这里应该将inode标记为可删除 */
			/* 但在此简化版本中，我们暂不实现此功能 */
	}
	
	return 0;
}

/* 写回inode到存储 */
static int ramfs_write_back_inode(struct inode *inode) {
	/* 在内存文件系统中，我们不需要真正写回存储 */
	/* 只需更新内存中的inode信息 */
	struct ramfs_inode_info *inode_info = inode->i_private;
	struct ramfs_inode *mem_inode = inode_info->mem_inode;
	
	/* 更新内存inode信息 */
	mem_inode->size = inode->i_size;
	mem_inode->nlinks = inode->i_nlink;
	
	/* 对于目录，需要更新其目录数据 */
	if (S_ISDIR(inode->i_mode)) {
			if (inode_info->dir_data) {
					/* 目录数据可能已经在内存中更新过，此处无需额外操作 */
			}
	}
	
	return 0;
}

/* 读取目录内容 */
static int ramfs_readdir(struct inode *dir, struct dir *dir_out, int *offset) {
	struct ramfs_inode_info *dir_info = dir->i_private;
	struct ramfs_direntry *entries = dir_info->dir_data;
	uint64 entry_count = dir->i_size / sizeof(struct ramfs_direntry);
	
	/* 检查偏移量是否有效 */
	if (*offset < 0 || (uint64)*offset >= entry_count) {
			return 0;  // 没有更多条目
	}
	
	/* 获取当前条目 */
	struct ramfs_direntry *current_entry = &entries[*offset];
	
	/* 填充输出结构 */
	dir_out->inum = current_entry->inum;
	strncpy(dir_out->name, current_entry->name, MAX_FILE_NAME_LEN - 1);
	dir_out->name[MAX_FILE_NAME_LEN - 1] = '\0';
	
	/* 增加偏移量 */
	(*offset)++;
	
	return 1;  // 成功读取一个条目
}