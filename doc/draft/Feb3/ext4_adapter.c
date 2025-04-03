/**
 * @file ext4_adapter.c
 * @brief Adapter layer between VFS and lwext4
 *
 * This file implements the adaptation layer between the VFS layer and the
 * lwext4 filesystem library. It translates VFS calls into lwext4 operations.
 */

 #include <kernel/fs/vfs/file.h>
 #include <kernel/fs/vfs/inode.h>
 #include <kernel/fs/vfs/dentry.h>
 #include <kernel/fs/vfs/superblock.h>
 #include <kernel/fs/vfs/buffer_head.h>
 #include <kernel/fs/lwext4/ext4.h>
 #include <kernel/fs/lwext4/ext4_config.h>
 #include <kernel/fs/lwext4/ext4_blockdev.h>
 #include <kernel/util/string.h>
 #include <kernel/mm/vma.h>
 
 #include <errno.h>
 #include <stdbool.h>
 #include <stdlib.h>
 
 #define EXT4_FS_NAME "ext4"
 #define EXT4_MAGIC 0xEF53
 
 /* Internal structure for ext4 filesystem info */
 struct ext4_fs_info {
	 char mount_point[255];         /* Mount point path */
	 char device_name[64];          /* Device name */
	 struct ext4_blockdev *bdev;    /* Block device */
	 struct ext4_sblock *sb;        /* Superblock */
	 struct ext4_lock locks;        /* Synchronization locks */
	 bool read_only;                /* Read-only mount flag */
 };
 
 /* Internal structure for ext4 inode info */
 struct ext4_inode_info {
	 uint32_t inode_no;             /* Inode number */
	 struct ext4_inode raw_inode;   /* Raw ext4 inode */
	 uint8_t state;                 /* Inode state */
 };
 
 /* Internal structure for ext4 file info */
 struct ext4_file_info {
	 ext4_file lwext4_file;         /* lwext4 file handle */
	 bool is_dir;                   /* Is this a directory? */
 };
 
 /* Forward declarations */
 static struct inode_operations ext4_dir_inode_operations;
 static struct inode_operations ext4_file_inode_operations;
 static struct inode_operations ext4_symlink_inode_operations;
 static struct file_operations ext4_dir_operations;
 static struct file_operations ext4_file_operations;
 static struct superblock_operations ext4_sops;
 
 /* Helper functions */
 
 /**
  * Convert VFS mode to lwext4 flags
  */
 static uint32_t vfs_mode_to_ext4_flags(fmode_t mode)
 {
	 uint32_t flags = 0;
	 
	 if (mode & FMODE_READ)
		 flags |= O_RDONLY;
	 if (mode & FMODE_WRITE)
		 flags |= O_WRONLY;
	 if ((mode & FMODE_READ) && (mode & FMODE_WRITE))
		 flags = O_RDWR;
	 if (mode & FMODE_APPEND)
		 flags |= O_APPEND;
	 if (mode & FMODE_NONBLOCK)
		 flags |= O_NONBLOCK;
	 
	 return flags;
 }
 
 /**
  * Get full path of a dentry within the mounted filesystem
  */
 static char* ext4_build_path(struct dentry *dentry, struct ext4_fs_info *fs_info)
 {
	 char *path = dentry_allocPath2Mount(dentry);
	 if (!path)
		 return NULL;
	 
	 /* If path is just "/", use empty string which means root in lwext4 */
	 if (strcmp(path, "/") == 0) {
		 free(path);
		 path = malloc(1);
		 if (path)
			 path[0] = '\0';
	 }
	 
	 return path;
 }
 
 /* Inode operations */
 
 /**
  * Look up a file or directory within a directory
  */
 static struct dentry* ext4_lookup(struct inode *dir, struct dentry *dentry, uint32 lookup_flags)
 {
	 struct ext4_fs_info *fs_info = (struct ext4_fs_info *)dir->i_superblock->s_fs_info;
	 struct ext4_inode_info *dir_info = (struct ext4_inode_info *)dir->i_fs_info;
	 char *path;
	 int ret;
	 
	 /* Build full path */
	 path = ext4_build_path(dentry, fs_info);
	 if (!path)
		 return NULL;
	 
	 /* Check if file/directory exists */
	 ret = ext4_inode_exist(path, EXT4_DE_UNKNOWN);
	 if (ret != 0) {
		 free(path);
		 return NULL; /* File not found */
	 }
	 
	 /* Get inode information */
	 struct inode *inode = inode_acquire(dir->i_superblock, 0); /* Temporary inode number, will be updated */
	 if (!inode) {
		 free(path);
		 return NULL;
	 }
	 
	 /* Create inode info */
	 struct ext4_inode_info *inode_info = malloc(sizeof(struct ext4_inode_info));
	 if (!inode_info) {
		 inode_unref(inode);
		 free(path);
		 return NULL;
	 }
	 
	 /* Fill inode info from lwext4 */
	 uint32_t ino;
	 ret = ext4_raw_inode_fill(path, &ino, &inode_info->raw_inode);
	 if (ret != 0) {
		 free(inode_info);
		 inode_unref(inode);
		 free(path);
		 return NULL;
	 }
	 
	 /* Set up inode */
	 inode_info->inode_no = ino;
	 inode->i_ino = ino;
	 inode->i_fs_info = inode_info;
	 
	 /* Set mode and permissions */
	 inode->i_mode = inode_info->raw_inode.mode;
	 
	 /* Set size and other attributes */
	 inode->i_size = inode_info->raw_inode.size;
	 inode->i_atime.tv_sec = inode_info->raw_inode.atime;
	 inode->i_mtime.tv_sec = inode_info->raw_inode.mtime;
	 inode->i_ctime.tv_sec = inode_info->raw_inode.ctime;
	 inode->i_nlink = inode_info->raw_inode.links_count;
	 
	 /* Set appropriate operations based on file type */
	 if (S_ISDIR(inode->i_mode)) {
		 inode->i_op = &ext4_dir_inode_operations;
		 inode->i_fop = &ext4_dir_operations;
	 } else if (S_ISREG(inode->i_mode)) {
		 inode->i_op = &ext4_file_inode_operations;
		 inode->i_fop = &ext4_file_operations;
	 } else if (S_ISLNK(inode->i_mode)) {
		 inode->i_op = &ext4_symlink_inode_operations;
		 /* Symlinks don't have file operations */
	 }
	 
	 /* Associate inode with dentry */
	 dentry_instantiate(dentry, inode);
	 
	 free(path);
	 return dentry;
 }
 
 /**
  * Create a new regular file
  */
 static struct inode* ext4_create(struct inode *dir, struct dentry *dentry, fmode_t mode, bool excl)
 {
	 struct ext4_fs_info *fs_info = (struct ext4_fs_info *)dir->i_superblock->s_fs_info;
	 struct ext4_inode_info *dir_info = (struct ext4_inode_info *)dir->i_fs_info;
	 char *path;
	 int ret;
	 
	 /* Cannot create files on read-only filesystem */
	 if (fs_info->read_only)
		 return NULL;
	 
	 /* Build full path */
	 path = ext4_build_path(dentry, fs_info);
	 if (!path)
		 return NULL;
	 
	 /* Create file using lwext4 */
	 ext4_file file;
	 ret = ext4_fopen(&file, path, "w+");
	 if (ret != 0) {
		 free(path);
		 return NULL;
	 }
	 
	 /* Close file immediately */
	 ext4_fclose(&file);
	 
	 /* Set file permissions */
	 ret = ext4_mode_set(path, mode & 0777);
	 if (ret != 0) {
		 /* Failed to set permissions, try to remove the file */
		 ext4_fremove(path);
		 free(path);
		 return NULL;
	 }
	 
	 /* Get the newly created inode */
	 uint32_t ino;
	 struct ext4_inode raw_inode;
	 ret = ext4_raw_inode_fill(path, &ino, &raw_inode);
	 if (ret != 0) {
		 free(path);
		 return NULL;
	 }
	 
	 /* Create VFS inode */
	 struct inode *inode = inode_acquire(dir->i_superblock, ino);
	 if (!inode) {
		 free(path);
		 return NULL;
	 }
	 
	 /* Create inode info */
	 struct ext4_inode_info *inode_info = malloc(sizeof(struct ext4_inode_info));
	 if (!inode_info) {
		 inode_unref(inode);
		 free(path);
		 return NULL;
	 }
	 
	 /* Set up inode info */
	 inode_info->inode_no = ino;
	 memcpy(&inode_info->raw_inode, &raw_inode, sizeof(struct ext4_inode));
	 inode->i_fs_info = inode_info;
	 
	 /* Set inode attributes */
	 inode->i_mode = raw_inode.mode;
	 inode->i_size = 0;
	 inode->i_atime.tv_sec = raw_inode.atime;
	 inode->i_mtime.tv_sec = raw_inode.mtime;
	 inode->i_ctime.tv_sec = raw_inode.ctime;
	 inode->i_nlink = 1;
	 
	 /* Set operations */
	 inode->i_op = &ext4_file_inode_operations;
	 inode->i_fop = &ext4_file_operations;
	 
	 /* Associate inode with dentry */
	 dentry_instantiate(dentry, inode);
	 
	 free(path);
	 return inode;
 }
 
 /**
  * Create a directory
  */
 static int32 ext4_mkdir(struct inode *dir, struct dentry *dentry, fmode_t mode)
 {
	 struct ext4_fs_info *fs_info = (struct ext4_fs_info *)dir->i_superblock->s_fs_info;
	 char *path;
	 int ret;
	 
	 /* Cannot create directories on read-only filesystem */
	 if (fs_info->read_only)
		 return -EROFS;
	 
	 /* Build full path */
	 path = ext4_build_path(dentry, fs_info);
	 if (!path)
		 return -ENOMEM;
	 
	 /* Create directory using lwext4 */
	 ret = ext4_dir_mk(path);
	 if (ret != 0) {
		 free(path);
		 return -EIO;
	 }
	 
	 /* Set directory permissions */
	 ret = ext4_mode_set(path, (mode & 0777) | S_IFDIR);
	 if (ret != 0) {
		 /* Try to remove the directory on failure */
		 ext4_dir_rm(path);
		 free(path);
		 return -EIO;
	 }
	 
	 /* Get inode information */
	 uint32_t ino;
	 struct ext4_inode raw_inode;
	 ret = ext4_raw_inode_fill(path, &ino, &raw_inode);
	 if (ret != 0) {
		 free(path);
		 return -EIO;
	 }
	 
	 /* Create VFS inode */
	 struct inode *inode = inode_acquire(dir->i_superblock, ino);
	 if (!inode) {
		 free(path);
		 return -ENOMEM;
	 }
	 
	 /* Create inode info */
	 struct ext4_inode_info *inode_info = malloc(sizeof(struct ext4_inode_info));
	 if (!inode_info) {
		 inode_unref(inode);
		 free(path);
		 return -ENOMEM;
	 }
	 
	 /* Set up inode info */
	 inode_info->inode_no = ino;
	 memcpy(&inode_info->raw_inode, &raw_inode, sizeof(struct ext4_inode));
	 inode->i_fs_info = inode_info;
	 
	 /* Set inode attributes */
	 inode->i_mode = raw_inode.mode;
	 inode->i_size = 0;
	 inode->i_atime.tv_sec = raw_inode.atime;
	 inode->i_mtime.tv_sec = raw_inode.mtime;
	 inode->i_ctime.tv_sec = raw_inode.ctime;
	 inode->i_nlink = 2; /* . and .. */
	 
	 /* Set operations */
	 inode->i_op = &ext4_dir_inode_operations;
	 inode->i_fop = &ext4_dir_operations;
	 
	 /* Associate inode with dentry */
	 dentry_instantiate(dentry, inode);
	 
	 free(path);
	 return 0;
 }
 
 /**
  * Remove a directory
  */
 static int32 ext4_rmdir(struct inode *dir, struct dentry *dentry)
 {
	 struct ext4_fs_info *fs_info = (struct ext4_fs_info *)dir->i_superblock->s_fs_info;
	 struct inode *inode = dentry->d_inode;
	 char *path;
	 int ret;
	 
	 /* Cannot remove directories on read-only filesystem */
	 if (fs_info->read_only)
		 return -EROFS;
	 
	 /* Check if directory is empty */
	 if (!dentry_isEmptyDir(dentry))
		 return -ENOTEMPTY;
	 
	 /* Build full path */
	 path = ext4_build_path(dentry, fs_info);
	 if (!path)
		 return -ENOMEM;
	 
	 /* Remove directory using lwext4 */
	 ret = ext4_dir_rm(path);
	 free(path);
	 
	 if (ret != 0)
		 return -EIO;
	 
	 /* Update parent directory */
	 dir->i_mtime.tv_sec = dir->i_ctime.tv_sec = time(NULL);
	 
	 return 0;
 }
 
 /**
  * Create a hard link
  */
 static int32 ext4_link(struct dentry *old_dentry, struct inode *dir, struct dentry *new_dentry)
 {
	 struct ext4_fs_info *fs_info = (struct ext4_fs_info *)dir->i_superblock->s_fs_info;
	 struct inode *inode = old_dentry->d_inode;
	 char *old_path, *new_path;
	 int ret;
	 
	 /* Cannot create links on read-only filesystem */
	 if (fs_info->read_only)
		 return -EROFS;
	 
	 /* Cannot link directories */
	 if (S_ISDIR(inode->i_mode))
		 return -EPERM;
	 
	 /* Build full paths */
	 old_path = ext4_build_path(old_dentry, fs_info);
	 if (!old_path)
		 return -ENOMEM;
	 
	 new_path = ext4_build_path(new_dentry, fs_info);
	 if (!new_path) {
		 free(old_path);
		 return -ENOMEM;
	 }
	 
	 /* Create hard link using lwext4 */
	 ret = ext4_flink(old_path, new_path);
	 free(old_path);
	 free(new_path);
	 
	 if (ret != 0)
		 return -EIO;
	 
	 /* Update inode */
	 inode->i_nlink++;
	 inode->i_ctime.tv_sec = time(NULL);
	 
	 /* Associate inode with new dentry */
	 inode_ref(inode);
	 dentry_instantiate(new_dentry, inode);
	 
	 return 0;
 }
 
 /**
  * Remove a file
  */
 static int32 ext4_unlink(struct inode *dir, struct dentry *dentry)
 {
	 struct ext4_fs_info *fs_info = (struct ext4_fs_info *)dir->i_superblock->s_fs_info;
	 struct inode *inode = dentry->d_inode;
	 struct ext4_inode_info *inode_info = (struct ext4_inode_info *)inode->i_fs_info;
	 char *path;
	 int ret;
	 
	 /* Cannot remove files on read-only filesystem */
	 if (fs_info->read_only)
		 return -EROFS;
	 
	 /* Build full path */
	 path = ext4_build_path(dentry, fs_info);
	 if (!path)
		 return -ENOMEM;
	 
	 /* Remove file using lwext4 */
	 ret = ext4_fremove(path);
	 free(path);
	 
	 if (ret != 0)
		 return -EIO;
	 
	 /* Update parent directory */
	 dir->i_mtime.tv_sec = dir->i_ctime.tv_sec = time(NULL);
	 
	 /* Update inode */
	 inode->i_nlink--;
	 inode->i_ctime.tv_sec = time(NULL);
	 
	 return 0;
 }
 
 /**
  * Create a symbolic link
  */
 static int32 ext4_symlink(struct inode *dir, struct dentry *dentry, const char *symname)
 {
	 struct ext4_fs_info *fs_info = (struct ext4_fs_info *)dir->i_superblock->s_fs_info;
	 char *path;
	 int ret;
	 
	 /* Cannot create symlinks on read-only filesystem */
	 if (fs_info->read_only)
		 return -EROFS;
	 
	 /* Build full path */
	 path = ext4_build_path(dentry, fs_info);
	 if (!path)
		 return -ENOMEM;
	 
	 /* Create symlink using lwext4 */
	 ret = ext4_fsymlink(symname, path);
	 free(path);
	 
	 if (ret != 0)
		 return -EIO;
	 
	 /* Get inode information */
	 path = ext4_build_path(dentry, fs_info);
	 if (!path)
		 return -ENOMEM;
	 
	 uint32_t ino;
	 struct ext4_inode raw_inode;
	 ret = ext4_raw_inode_fill(path, &ino, &raw_inode);
	 if (ret != 0) {
		 free(path);
		 return -EIO;
	 }
	 
	 /* Create VFS inode */
	 struct inode *inode = inode_acquire(dir->i_superblock, ino);
	 if (!inode) {
		 free(path);
		 return -ENOMEM;
	 }
	 
	 /* Create inode info */
	 struct ext4_inode_info *inode_info = malloc(sizeof(struct ext4_inode_info));
	 if (!inode_info) {
		 inode_unref(inode);
		 free(path);
		 return -ENOMEM;
	 }
	 
	 /* Set up inode info */
	 inode_info->inode_no = ino;
	 memcpy(&inode_info->raw_inode, &raw_inode, sizeof(struct ext4_inode));
	 inode->i_fs_info = inode_info;
	 
	 /* Set inode attributes */
	 inode->i_mode = raw_inode.mode;
	 inode->i_size = strlen(symname);
	 inode->i_atime.tv_sec = raw_inode.atime;
	 inode->i_mtime.tv_sec = raw_inode.mtime;
	 inode->i_ctime.tv_sec = raw_inode.ctime;
	 inode->i_nlink = 1;
	 
	 /* Set operations */
	 inode->i_op = &ext4_symlink_inode_operations;
	 
	 /* Associate inode with dentry */
	 dentry_instantiate(dentry, inode);
	 
	 free(path);
	 return 0;
 }
 
 /**
  * Read a symbolic link
  */
 static int32 ext4_readlink(struct dentry *dentry, char *buffer, int buflen)
 {
	 struct ext4_fs_info *fs_info = (struct ext4_fs_info *)dentry->d_inode->i_superblock->s_fs_info;
	 char *path;
	 int ret;
	 size_t read_count;
	 
	 /* Build full path */
	 path = ext4_build_path(dentry, fs_info);
	 if (!path)
		 return -ENOMEM;
	 
	 /* Read symlink using lwext4 */
	 ret = ext4_readlink(path, buffer, buflen, &read_count);
	 free(path);
	 
	 if (ret != 0)
		 return -EIO;
	 
	 return read_count;
 }
 
 /**
  * Rename a file or directory
  */
 static int32 ext4_rename(struct inode *old_dir, struct dentry *old_dentry,
						  struct inode *new_dir, struct dentry *new_dentry, uint32 flags)
 {
	 struct ext4_fs_info *fs_info = (struct ext4_fs_info *)old_dir->i_superblock->s_fs_info;
	 char *old_path, *new_path;
	 int ret;
	 
	 /* Cannot rename files on read-only filesystem */
	 if (fs_info->read_only)
		 return -EROFS;
	 
	 /* Build full paths */
	 old_path = ext4_build_path(old_dentry, fs_info);
	 if (!old_path)
		 return -ENOMEM;
	 
	 new_path = ext4_build_path(new_dentry, fs_info);
	 if (!new_path) {
		 free(old_path);
		 return -ENOMEM;
	 }
	 
	 /* Rename using lwext4 */
	 ret = ext4_frename(old_path, new_path);
	 free(old_path);
	 free(new_path);
	 
	 if (ret != 0)
		 return -EIO;
	 
	 /* Update timestamps */
	 old_dir->i_mtime.tv_sec = old_dir->i_ctime.tv_sec = time(NULL);
	 if (old_dir != new_dir)
		 new_dir->i_mtime.tv_sec = new_dir->i_ctime.tv_sec = time(NULL);
	 
	 return 0;
 }
 
 /**
  * Permission check operation
  */
 static int32 ext4_permission(struct inode *inode, int32 mask)
 {
	 /* Basic permission check */
	 if ((mask & MAY_EXEC) && !S_ISDIR(inode->i_mode) && !(inode->i_mode & 0111))
		 return -EACCES;
	 
	 if ((mask & MAY_WRITE) && !(inode->i_mode & 0222))
		 return -EACCES;
	 
	 if ((mask & MAY_READ) && !(inode->i_mode & 0444))
		 return -EACCES;
	 
	 return 0;
 }
 
 /**
  * Get inode attributes
  */
 static int32 ext4_getattr(const struct path *path, struct kstat *stat, uint32 request_mask, uint32 flags)
 {
	 struct dentry *dentry = path->dentry;
	 struct inode *inode = dentry->d_inode;
	 struct ext4_inode_info *inode_info = (struct ext4_inode_info *)inode->i_fs_info;
	 
	 /* Fill in basic stats */
	 stat->dev = inode->i_superblock->s_device_id;
	 stat->ino = inode->i_ino;
	 stat->mode = inode->i_mode;
	 stat->nlink = inode->i_nlink;
	 stat->uid = inode->i_uid;
	 stat->gid = inode->i_gid;
	 stat->rdev = inode->i_rdev;
	 stat->size = inode->i_size;
	 stat->blksize = inode->i_superblock->s_blocksize;
	 stat->blocks = inode->i_blocks;
	 stat->atime = inode->i_atime;
	 stat->mtime = inode->i_mtime;
	 stat->ctime = inode->i_ctime;
	 
	 return 0;
 }
 
 /**
  * Update inode attributes
  */
 static int32 ext4_setattr(struct dentry *dentry, struct iattr *attr)
 {
	 struct ext4_fs_info *fs_info = (struct ext4_fs_info *)dentry->d_inode->i_superblock->s_fs_info;
	 struct inode *inode = dentry->d_inode;
	 char *path;
	 int ret;
	 
	 /* Cannot modify attributes on read-only filesystem */
	 if (fs_info->read_only)
		 return -EROFS;
	 
	 /* Check permissions */
	 ret = setattr_prepare(dentry, attr);
	 if (ret != 0)
		 return ret;
	 
	 /* Build full path */
	 path = ext4_build_path(dentry, fs_info);
	 if (!path)
		 return -ENOMEM;
	 
	 /* Update mode if requested */
	 if (attr->ia_valid & ATTR_MODE) {
		 ret = ext4_mode_set(path, attr->ia_mode & 0777);
		 if (ret != 0) {
			 free(path);
			 return -EIO;
		 }
		 inode->i_mode = (inode->i_mode & ~0777) | (attr->ia_mode & 0777);
	 }
	 
	 /* Update owner if requested */
	 if (attr->ia_valid & (ATTR_UID | ATTR_GID)) {
		 uint32_t uid = (attr->ia_valid & ATTR_UID) ? attr->ia_uid : inode->i_uid;
		 uint32_t gid = (attr->ia_valid & ATTR_GID) ? attr->ia_gid : inode->i_gid;
		 
		 ret = ext4_owner_set(path, uid, gid);
		 if (ret != 0) {
			 free(path);
			 return -EIO;
		 }
		 
		 if (attr->ia_valid & ATTR_UID)
			 inode->i_uid = attr->ia_uid;
		 if (attr->ia_valid & ATTR_GID)
			 inode->i_gid = attr->ia_gid;
	 }
	 
	 /* Update size if requested */
	 if (attr->ia_valid & ATTR_SIZE) {
		 /* For regular files only */
		 if (S_ISREG(inode->i_mode)) {
			 ext4_file file;
			 ret = ext4_fopen(&file, path, "r+");
			 if (ret != 0) {
				 free(path);
				 return -EIO;
			 }
			 
			 ret = ext4_ftruncate(&file, attr->ia_size);
			 ext4_fclose(&file);
			 
			 if (ret != 0) {
				 free(path);
				 return -EIO;
			 }
			 
			 inode->i_size = attr->ia_size;
		 }
	 }
	 
	 /* Update timestamps if requested */
	 if (attr->ia_valid & ATTR_ATIME) {
		 ret = ext4_atime_set(path, attr->ia_atime.tv_sec);
		 if (ret != 0) {
			 free(path);
			 return -EIO;
		 }
		 inode->i_atime = attr->ia_atime;
	 }
	 
	 if (attr->ia_valid & ATTR_MTIME) {
		 ret = ext4_mtime_set(path, attr->ia_mtime.tv_sec);
		 if (ret != 0) {
			 free(path);
			 return -EIO;
		 }
		 inode->i_mtime = attr->ia_mtime;
	 }
	 
	 if (attr->ia_valid & ATTR_CTIME) {
		 ret = ext4_ctime_set(path, attr->ia_ctime.tv_sec);
		 if (ret != 0) {
			 free(path);
			 return -EIO;
		 }
		 inode->i_ctime = attr->ia_ctime;
	 }
	 
	 free(path);
	 return 0;
 }
 
 /* File operations */
 
 /**
  * Open a file
  */
 static int32 ext4_file_open(struct file *file, int32 flags)
 {
	 struct ext4_fs_info *fs_info = (struct ext4_fs_info *)file->f_dentry->d_inode->i_superblock->s_fs_info;
	 struct inode *inode = file->f_inode;
	 char *path;
	 int ret;
	 
	 /* Check if write access is requested on read-only filesystem */
	 if (fs_info->read_only && (flags & O_ACCMODE) != O_RDONLY)
		 return -EROFS;
	 
	 /* Build full path */
	 path = ext4_build_path(file->f_dentry, fs_info);
	 if (!path)
		 return -ENOMEM;
	 
	 /* Allocate file info */
	 struct ext4_file_info *file_info = malloc(sizeof(struct ext4_file_info));
	 if (!file_info) {
		 free(path);
		 return -ENOMEM;
	 }
	 
	 /* Open file using lwext4 */
	 const char *mode;
	 if ((flags & O_ACCMODE) == O_RDONLY)
		 mode = "r";
	 else if ((flags & O_ACCMODE) == O_WRONLY)
		 mode = (flags & O_APPEND) ? "a" : "w";
	 else /* O_RDWR */
		 mode = (flags & O_APPEND) ? "a+" : "r+";
	 
	 ret = ext4_fopen(&file_info->lwext4_file, path, mode);
	 free(path);
	 
	 if (ret != 0) {
		 free(file_info);
		 return -EIO;
	 }
	 
	 /* Set file position based on flags */
	 if (flags & O_APPEND)
		 file->f_pos = ext4_fsize(&file_info->lwext4_file);
	 else
		 file->f_pos = 0;
	 
	 file_info->is_dir = S_ISDIR(inode->i_mode);
	 file->f_private = file_info;
	 
	 return 0;
 }
 
 /**
  * Close a file
  */
 static int32 ext4_file_release(struct file *file)
 {
	 struct ext4_file_info *file_info = (struct ext4_file_info *)file->f_private;
	 int ret;
	 
	 if (!file_info)
		 return 0;
	 
	 if (!file_info->is_dir) {
		 /* Close regular file */
		 ret = ext4_fclose(&file_info->lwext4_file);
		 if (ret != 0)
			 return -EIO;
	 }
	 
	 free(file_info);
	 file->f_private = NULL;
	 
	 return 0;
 }
 

 /**
  * Write to a file
  */
 static ssize_t ext4_file_write(struct file *file, const char *buf, size_t count, loff_t *pos)
 {
	 struct ext4_fs_info *fs_info = (struct ext4_fs_info *)file->f_dentry->d_inode->i_superblock->s_fs_info;
	 struct ext4_file_info *file_info = (struct ext4_file_info *)file->f_private;
	 struct inode *inode = file->f_inode;
	 size_t bytes_written;
	 int ret;
	 
	 /* Cannot write to read-only filesystem */
	 if (fs_info->read_only)
		 return -EROFS;
	 
	 /* Cannot write to a directory */
	 if (file_info->is_dir)
		 return -EISDIR;
	 
	 /* Set file position */
	 ret = ext4_fseek(&file_info->lwext4_file, *pos, SEEK_SET);
	 if (ret != 0)
		 return -EIO;
	 
	 /* Write to file */
	 ret = ext4_fwrite(&file_info->lwext4_file, buf, count, &bytes_written);
	 if (ret != 0)
		 return -EIO;
	 
	 /* Update position */
	 *pos += bytes_written;
	 
	 /* Update inode size if necessary */
	 if (*pos > inode->i_size) {
		 inode->i_size = *pos;
	 }
	 
	 /* Update timestamps */
	 inode->i_mtime.tv_sec = inode->i_ctime.tv_sec = time(NULL);
	 
	 return bytes_written;
 }
 
 /**
  * Change file position
  */
 static loff_t ext4_file_llseek(struct file *file, loff_t offset, int32 whence)
 {
	 struct ext4_file_info *file_info = (struct ext4_file_info *)file->f_private;
	 struct inode *inode = file->f_inode;
	 loff_t new_pos;
	 int ret;
	 
	 /* Cannot seek in a directory */
	 if (file_info->is_dir)
		 return -EISDIR;
	 
	 /* Calculate new position */
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
			 return -EINVAL;
	 }
	 
	 /* Check for negative position */
	 if (new_pos < 0)
		 return -EINVAL;
	 
	 /* Update lwext4 file position */
	 ret = ext4_fseek(&file_info->lwext4_file, new_pos, SEEK_SET);
	 if (ret != 0)
		 return -EIO;
	 
	 /* Update VFS file position */
	 file->f_pos = new_pos;
	 
	 return new_pos;
 }
 
 /**
  * Synchronize file state
  */
 static int32 ext4_file_fsync(struct file *file, loff_t start, loff_t end, int32 datasync)
 {
	 struct ext4_fs_info *fs_info = (struct ext4_fs_info *)file->f_dentry->d_inode->i_superblock->s_fs_info;
	 int ret;
	 
	 /* Flush filesystem cache */
	 ret = ext4_cache_flush(fs_info->mount_point);
	 if (ret != 0)
		 return -EIO;
	 
	 return 0;
 }
 
 /**
  * Read directory entries
  */
 static int32 ext4_dir_iterate(struct file *file, struct dir_context *ctx)
 {
	 struct ext4_fs_info *fs_info = (struct ext4_fs_info *)file->f_dentry->d_inode->i_superblock->s_fs_info;
	 struct inode *inode = file->f_inode;
	 char *path;
	 int ret;
	 
	 /* Must be a directory */
	 if (!S_ISDIR(inode->i_mode))
		 return -ENOTDIR;
	 
	 /* Build full path */
	 path = ext4_build_path(file->f_dentry, fs_info);
	 if (!path)
		 return -ENOMEM;
	 
	 /* Open directory */
	 ext4_dir dir;
	 ret = ext4_dir_open(&dir, path);
	 free(path);
	 
	 if (ret != 0)
		 return -EIO;
	 
	 /* Skip to current position */
	 ext4_dir_entry_rewind(&dir);
	 loff_t pos = 0;
	 
	 const ext4_direntry *entry;
	 while ((entry = ext4_dir_entry_next(&dir)) != NULL) {
		 /* Skip entries until we reach ctx->pos */
		 if (pos < ctx->pos) {
			 pos++;
			 continue;
		 }
		 
		 /* Determine file type */
		 uint32_t file_type;
		 if (entry->inode_type == EXT4_DE_REG_FILE)
			 file_type = DT_REG;
		 else if (entry->inode_type == EXT4_DE_DIR)
			 file_type = DT_DIR;
		 else if (entry->inode_type == EXT4_DE_CHRDEV)
			 file_type = DT_CHR;
		 else if (entry->inode_type == EXT4_DE_BLKDEV)
			 file_type = DT_BLK;
		 else if (entry->inode_type == EXT4_DE_FIFO)
			 file_type = DT_FIFO;
		 else if (entry->inode_type == EXT4_DE_SOCK)
			 file_type = DT_SOCK;
		 else if (entry->inode_type == EXT4_DE_SYMLINK)
			 file_type = DT_LNK;
		 else
			 file_type = DT_UNKNOWN;
		 
		 /* Call the actor function */
		 if (!ctx->actor(ctx, (const char *)entry->name, entry->name_length, pos, entry->inode, file_type))
			 break;
		 
		 /* Update position */
		 pos++;
		 ctx->pos = pos;
	 }
	 
	 /* Close directory */
	 ext4_dir_close(&dir);
	 
	 return 0;
 }
 
 /* Superblock operations */
 
 /**
  * Allocate an inode
  */
 static struct inode* ext4_alloc_inode(struct superblock *sb, uint64 ino)
 {
	 struct inode *inode;
	 struct ext4_inode_info *inode_info;
	 
	 /* Allocate basic inode */
	 inode = malloc(sizeof(struct inode));
	 if (!inode)
		 return NULL;
	 
	 /* Initialize inode */
	 memset(inode, 0, sizeof(struct inode));
	 inode->i_superblock = sb;
	 inode->i_ino = ino;
	 atomic_set(&inode->i_refcount, 1);
	 
	 /* Allocate inode info */
	 inode_info = malloc(sizeof(struct ext4_inode_info));
	 if (!inode_info) {
		 free(inode);
		 return NULL;
	 }
	 
	 /* Initialize inode info */
	 memset(inode_info, 0, sizeof(struct ext4_inode_info));
	 inode_info->inode_no = ino;
	 inode->i_fs_info = inode_info;
	 
	 return inode;
 }
 
 /**
  * Destroy an inode
  */
 static void ext4_destroy_inode(struct inode *inode)
 {
	 struct ext4_inode_info *inode_info = (struct ext4_inode_info *)inode->i_fs_info;
	 
	 if (inode_info)
		 free(inode_info);
	 
	 free(inode);
 }
 
 /**
  * Write an inode to disk
  */
 static int32 ext4_write_inode(struct inode *inode, int32 wait)
 {
	 struct ext4_fs_info *fs_info = (struct ext4_fs_info *)inode->i_superblock->s_fs_info;
	 struct ext4_inode_info *inode_info = (struct ext4_inode_info *)inode->i_fs_info;
	 
	 /* Cannot write to read-only filesystem */
	 if (fs_info->read_only)
		 return -EROFS;
	 
	 /* Currently, lwext4 doesn't provide a direct way to write an inode */
	 
	 return 0;
 }
 
 /**
  * Read an inode from disk
  */
 static int32 ext4_read_inode(struct inode *inode)
 {
	 struct ext4_fs_info *fs_info = (struct ext4_fs_info *)inode->i_superblock->s_fs_info;
	 struct ext4_inode_info *inode_info = (struct ext4_inode_info *)inode->i_fs_info;
	 uint32_t ino = inode->i_ino;
	 
	 /* Currently, lwext4 doesn't provide a direct way to read an inode by number */
	 
	 return 0;
 }
 
 /**
  * Synchronize superblock to disk
  */
 static int32 ext4_sync_fs(struct superblock *sb, int32 wait)
 {
	 struct ext4_fs_info *fs_info = (struct ext4_fs_info *)sb->s_fs_info;
	 int ret;
	 
	 /* Cannot write to read-only filesystem */
	 if (fs_info->read_only)
		 return -EROFS;
	 
	 /* Flush cache */
	 ret = ext4_cache_flush(fs_info->mount_point);
	 if (ret != 0)
		 return -EIO;
	 
	 return 0;
 }
 
 /**
  * Get filesystem statistics
  */
 static int32 ext4_statfs(struct superblock *sb, struct statfs *statfs)
 {
	 struct ext4_fs_info *fs_info = (struct ext4_fs_info *)sb->s_fs_info;
	 struct ext4_mount_stats stats;
	 int ret;
	 
	 /* Get filesystem stats */
	 ret = ext4_mount_point_stats(fs_info->mount_point, &stats);
	 if (ret != 0)
		 return -EIO;
	 
	 /* Fill in statfs structure */
	 statfs->f_type = EXT4_MAGIC;
	 statfs->f_bsize = stats.block_size;
	 statfs->f_blocks = stats.blocks_count;
	 statfs->f_bfree = stats.free_blocks_count;
	 statfs->f_bavail = stats.free_blocks_count;
	 statfs->f_files = stats.inodes_count;
	 statfs->f_ffree = stats.free_inodes_count;
	 statfs->f_namelen = 255; /* Maximum filename length in ext4 */
	 statfs->f_frsize = stats.block_size;
	 
	 return 0;
 }
 
 /**
  * Clean up and unmount filesystem
  */
 static void ext4_put_super(struct superblock *sb)
 {
	 struct ext4_fs_info *fs_info = (struct ext4_fs_info *)sb->s_fs_info;
	 
	 if (fs_info) {
		 /* Unmount filesystem */
		 ext4_umount(fs_info->mount_point);
		 
		 /* Unregister block device */
		 ext4_device_unregister(fs_info->device_name);
		 
		 /* Free filesystem info */
		 free(fs_info);
		 sb->s_fs_info = NULL;
	 }
 }
 
 /* Define all operations structures */
 
 static struct inode_operations ext4_dir_inode_operations = {
	 .lookup     = ext4_lookup,
	 .create     = ext4_create,
	 .link       = ext4_link,
	 .unlink     = ext4_unlink,
	 .symlink    = ext4_symlink,
	 .mkdir      = ext4_mkdir,
	 .rmdir      = ext4_rmdir,
	 .rename     = ext4_rename,
	 .permission = ext4_permission,
	 .getattr    = ext4_getattr,
	 .setattr    = ext4_setattr,
 };
 
 static struct inode_operations ext4_file_inode_operations = {
	 .permission = ext4_permission,
	 .getattr    = ext4_getattr,
	 .setattr    = ext4_setattr,
 };
 
 static struct inode_operations ext4_symlink_inode_operations = {
	 .readlink   = ext4_readlink,
	 .permission = ext4_permission,
	 .getattr    = ext4_getattr,
	 .setattr    = ext4_setattr,
 };
 
 static struct file_operations ext4_dir_operations = {
	 .open       = ext4_file_open,
	 .release    = ext4_file_release,
	 .iterate    = ext4_dir_iterate,
 };
 
 static struct file_operations ext4_file_operations = {
	 .open       = ext4_file_open,
	 .release    = ext4_file_release,
	 .llseek     = ext4_file_llseek,
	 .fsync      = ext4_file_fsync,
 };
 
 static struct superblock_operations ext4_sops = {
	 .alloc_inode  = ext4_alloc_inode,
	 .destroy_inode = ext4_destroy_inode,
	 .write_inode  = ext4_write_inode,
	 .read_inode   = ext4_read_inode,
	 .sync_fs      = ext4_sync_fs,
	 .statfs       = ext4_statfs,
	 .put_super    = ext4_put_super,
 };
 
 /* Missing file operations implementation for reading and writing */
 ssize_t ext4_read(struct file *file, char *buf, size_t size, loff_t *pos)
 {
	 return ext4_file_read(file, buf, size, pos);
 }
 
 ssize_t ext4_write(struct file *file, const char *buf, size_t size, loff_t *pos)
 {
	 return ext4_file_write(file, buf, size, pos);
 }
 
 /* Filesystem type registration */
 
 /**
  * Mount ext4 filesystem
  */
 static struct superblock* ext4_mount_fs(struct fstype *type, int32 flags, dev_t dev_id, const void *fs_data)
 {
	 struct superblock *sb;
	 struct ext4_fs_info *fs_info;
	 int ret;
	 const char *dev_name;
	 const char *mount_point;
	 bool read_only = (flags & MS_RDONLY) != 0;
	 
	 /* Get device name and mount point from fs_data */
	 if (!fs_data)
		 return NULL;
	 
	 dev_name = ((const char **)fs_data)[0];
	 mount_point = ((const char **)fs_data)[1];
	 
	 if (!dev_name || !mount_point)
		 return NULL;
	 
	 /* Create superblock */
	 sb = malloc(sizeof(struct superblock));
	 if (!sb)
		 return NULL;
	 
	 /* Initialize superblock */
	 memset(sb, 0, sizeof(struct superblock));
	 sb->s_device_id = dev_id;
	 sb->s_fstype = type;
	 sb->s_operations = &ext4_sops;
	 sb->s_magic = EXT4_MAGIC;
	 atomic_set(&sb->s_refcount, 1);
	 
	 /* Create filesystem info */
	 fs_info = malloc(sizeof(struct ext4_fs_info));
	 if (!fs_info) {
		 free(sb);
		 return NULL;
	 }
	 
	 /* Initialize filesystem info */
	 memset(fs_info, 0, sizeof(struct ext4_fs_info));
	 strncpy(fs_info->device_name, dev_name, sizeof(fs_info->device_name) - 1);
	 strncpy(fs_info->mount_point, mount_point, sizeof(fs_info->mount_point) - 1);
	 fs_info->read_only = read_only;
	 
	 /* Initialize locks */
	 fs_info->locks.lock = NULL;    /* No locking for now */
	 fs_info->locks.unlock = NULL;  /* No locking for now */
	 
	 /* Register block device */
	 ret = ext4_device_register(fs_info->bdev, fs_info->device_name);
	 if (ret != 0) {
		 free(fs_info);
		 free(sb);
		 return NULL;
	 }
	 
	 /* Mount filesystem */
	 ret = ext4_mount(fs_info->device_name, fs_info->mount_point, read_only);
	 if (ret != 0) {
		 ext4_device_unregister(fs_info->device_name);
		 free(fs_info);
		 free(sb);
		 return NULL;
	 }
	 
	 /* Get ext4 superblock info */
	 ret = ext4_get_sblock(fs_info->mount_point, &fs_info->sb);
	 if (ret != 0) {
		 ext4_umount(fs_info->mount_point);
		 ext4_device_unregister(fs_info->device_name);
		 free(fs_info);
		 free(sb);
		 return NULL;
	 }
	 
	 /* Set up superblock */
	 sb->s_fs_info = fs_info;
	 sb->s_blocksize = fs_info->sb->block_size;
	 sb->s_blocksize_bits = 0;
	 uint64_t tmp = sb->s_blocksize;
	 while (tmp >>= 1)
		 sb->s_blocksize_bits++;
	 
	 /* Create root inode and dentry */
	 struct inode *root_inode = inode_acquire(sb, EXT4_ROOT_INO);
	 if (!root_inode) {
		 ext4_umount(fs_info->mount_point);
		 ext4_device_unregister(fs_info->device_name);
		 free(fs_info);
		 free(sb);
		 return NULL;
	 }
	 
	 /* Create root dentry */
	 struct dentry *root_dentry = malloc(sizeof(struct dentry));
	 if (!root_dentry) {
		 inode_unref(root_inode);
		 ext4_umount(fs_info->mount_point);
		 ext4_device_unregister(fs_info->device_name);
		 free(fs_info);
		 free(sb);
		 return NULL;
	 }
	 
	 /* Initialize root dentry */
	 memset(root_dentry, 0, sizeof(struct dentry));
	 root_dentry->d_inode = root_inode;
	 root_dentry->d_name = malloc(sizeof(struct qstr));
	 if (!root_dentry->d_name) {
		 free(root_dentry);
		 inode_unref(root_inode);
		 ext4_umount(fs_info->mount_point);
		 ext4_device_unregister(fs_info->device_name);
		 free(fs_info);
		 free(sb);
		 return NULL;
	 }
	 
	 /* Initialize root dentry name */
	 root_dentry->d_name->name = strdup("/");
	 root_dentry->d_name->len = 1;
	 root_dentry->d_name->hash = 0;
	 root_dentry->d_superblock = sb;
	 
	 /* Set up inode info */
	 struct ext4_inode_info *inode_info = malloc(sizeof(struct ext4_inode_info));
	 if (!inode_info) {
		 free((void *)root_dentry->d_name->name);
		 free(root_dentry->d_name);
		 free(root_dentry);
		 inode_unref(root_inode);
		 ext4_umount(fs_info->mount_point);
		 ext4_device_unregister(fs_info->device_name);
		 free(fs_info);
		 free(sb);
		 return NULL;
	 }
	 
	 /* Get inode information from lwext4 */
	 memset(inode_info, 0, sizeof(struct ext4_inode_info));
	 inode_info->inode_no = EXT4_ROOT_INO;
	 
	 /* Fill root inode */
	 ext4_raw_inode_fill("", &inode_info->inode_no, &inode_info->raw_inode);
	 root_inode->i_fs_info = inode_info;
	 root_inode->i_mode = inode_info->raw_inode.mode;
	 root_inode->i_size = inode_info->raw_inode.size;
	 root_inode->i_atime.tv_sec = inode_info->raw_inode.atime;
	 root_inode->i_mtime.tv_sec = inode_info->raw_inode.mtime;
	 root_inode->i_ctime.tv_sec = inode_info->raw_inode.ctime;
	 root_inode->i_nlink = inode_info->raw_inode.links_count;
	 
	 /* Set operations */
	 root_inode->i_op = &ext4_dir_inode_operations;
	 root_inode->i_fop = &ext4_dir_operations;
	 
	 /* Set superblock root */
	 sb->s_root = root_dentry;
	 
	 return sb;
 }
 
 /**
  * Kill ext4 superblock
  */
 static void ext4_kill_sb(struct superblock *sb)