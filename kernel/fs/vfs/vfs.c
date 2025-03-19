#include <kernel/fs/vfs.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/sched/sched.h>
#include <kernel/types.h>
#include <util/string.h>

/**
 * vfs_kern_mount - Mount a filesystem without adding to namespace
 * @type: Filesystem type to mount
 * @flags: Mount flags
 * @name: Device name to mount
 * @data: Filesystem-specific data
 *
 * Creates a mount point for a filesystem of the specified type.
 * This performs the mount operation but doesn't add the mount
 * point to any namespace - it's a kernel-internal mount.
 *
 * Returns a vfsmount structure on success, NULL on failure.
 */
struct vfsmount* vfs_kern_mount(struct fs_type* type, int flags,
				const char* name, void* data) {
	struct vfsmount* mnt;
	struct super_block* sb;
	int error;
	static int mount_id = 0;

	if (!type)
		return NULL;

	/* Get or create a superblock */
	sb = mount_fs(type, flags, name, data);
	if (IS_ERR(sb))
		return NULL;

	/* Create new mount structure */
	mnt = kmalloc(sizeof(struct vfsmount));
	if (!mnt) {
		deactivate_super_safe(sb);
		drop_super(sb);
		return NULL;
	}

	/* Initialize the mount structure */
	memset(mnt, 0, sizeof(struct vfsmount));
	atomic_set(&mnt->mnt_refcount, 1);
	mnt->mnt_superblock = sb;
	mnt->mnt_root = dget(sb->sb_global_root_dentry);
	mnt->mnt_flags = flags;
	mnt->mnt_id = mount_id++;

	/* Initialize list nodes */
	INIT_LIST_HEAD(&mnt->mnt_node_superblock);
	INIT_LIST_HEAD(&mnt->mnt_node_global);
	INIT_LIST_HEAD(&mnt->mnt_node_namespace);

	/* Add to superblock's mount list */
	spin_lock(&sb->sb_list_mounts_lock);
	list_add(&mnt->mnt_node_superblock, &sb->sb_list_mounts);
	spin_unlock(&sb->sb_list_mounts_lock);

	return mnt;
}

/**
 * vfs_read - Read data from a file
 * @file: File to read from
 * @buf: Buffer to read into
 * @count: Number of bytes to read
 * @pos: Position pointer
 *
 * Returns bytes read or negative error code
 */
ssize_t vfs_read(struct file* file, char* buf, size_t count, loff_t* pos) {
	if (!file || !file->f_operations)
		return -EINVAL;

	if (!file->f_operations->read)
		return -EINVAL;

	/* Check permission */
	if (!(file->f_mode & FMODE_READ))
		return -EBADF;

	return file->f_operations->read(file, buf, count, pos);
}

/**
 * vfs_write - Write data to a file
 * @file: File to write to
 * @buf: Data to write
 * @count: Number of bytes to write
 * @pos: Position pointer
 *
 * Returns bytes written or negative error code
 */
ssize_t vfs_write(struct file* file, const char* buf, size_t count,
		  loff_t* pos) {
	if (!file || !file->f_operations)
		return -EINVAL;

	if (!file->f_operations->write)
		return -EINVAL;

	/* Check permission */
	if (!(file->f_mode & FMODE_WRITE))
		return -EBADF;

	return file->f_operations->write(file, buf, count, pos);
}

/**
 * vfs_mkdir - Create a directory
 * @dir: Parent directory's inode
 * @dentry: Dentry for the new directory
 * @mode: Permission mode
 *
 * Returns 0 on success or negative error code
 */
int vfs_mkdir(struct inode* dir, struct dentry* dentry, fmode_t mode) {
	int error;

	if (!dir || !dentry)
		return -EINVAL;

	if (!dir->i_op || !dir->i_op->mkdir)
		return -EPERM;

	/* Check if entry already exists */
	if (dentry->d_inode)
		return -EEXIST;

	/* Check directory permissions */
	error = inode_permission(dir, MAY_WRITE | MAY_EXEC);
	if (error)
		return error;

	return dir->i_op->mkdir(dir, dentry, mode & ~current_task()->umask);
}

/**
 * vfs_rmdir - Remove a directory
 * @dir: Parent directory's inode
 * @dentry: Directory to remove
 *
 * Returns 0 on success or negative error code
 */
int vfs_rmdir(struct inode* dir, struct dentry* dentry) {
	int error;

	if (!dir || !dentry || !dentry->d_inode)
		return -EINVAL;

	if (!dir->i_op || !dir->i_op->rmdir)
		return -EPERM;

	/* Check directory permissions */
	error = inode_permission(dir, MAY_WRITE | MAY_EXEC);
	if (error)
		return error;

	/* Cannot remove non-empty directory */
	if (!is_empty_dir(dentry->d_inode))
		return -ENOTEMPTY;

	return dir->i_op->rmdir(dir, dentry);
}

/**
 * vfs_link - Create a hard link
 * @old_dentry: Existing file's dentry
 * @dir: Directory to create link in
 * @new_dentry: Dentry for new link
 * @new_inode: Output parameter for resulting inode
 *
 * Returns 0 on success or negative error code
 */
int vfs_link(struct dentry* old_dentry, struct inode* dir,
	     struct dentry* new_dentry, struct inode** new_inode) {
	int error;

	if (!old_dentry || !dir || !new_dentry)
		return -EINVAL;

	if (!dir->i_op || !dir->i_op->link)
		return -EPERM;

	/* Check if target exists */
	if (new_dentry->d_inode)
		return -EEXIST;

	/* Check permissions */
	error = inode_permission(dir, MAY_WRITE);
	if (error)
		return error;

	error = dir->i_op->link(old_dentry, dir, new_dentry);
	if (error)
		return error;

	if (new_inode)
		*new_inode = new_dentry->d_inode;

	return 0;
}