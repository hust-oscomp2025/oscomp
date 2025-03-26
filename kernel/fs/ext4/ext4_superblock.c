#include <kernel/fs/ext4_adaptor.h>
#include <kernel/fs/vfs/superblock.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/mm/kmalloc.h>

#include <kernel/types.h>

/* Forward declarations */
static int ext4_read_inode(struct inode* inode);
static int ext4_write_inode(struct inode* inode, int wait);
static void ext4_put_super(struct superblock* sb);
static int ext4_statfs(struct superblock* sb, struct statfs* stat);
static int ext4_sync_fs(struct superblock* sb, int wait);

/* Define the superblock operations structure for ext4 */
const struct superblock_operations ext4_superblock_operations = {
    .read_inode = ext4_read_inode,
    .write_inode = ext4_write_inode,
    .put_super = ext4_put_super,
    .statfs = ext4_statfs,
    .sync_fs = ext4_sync_fs,
};

/**
 * ext4_read_inode - Read an inode from the filesystem
 * @inode: The inode to read
 *
 * Returns: 0 on success, negative error code on failure
 */
static int ext4_read_inode(struct inode* inode) {
	/* This is implemented in ext4_inode_operations.c */
	extern int ext4_inode_init(struct superblock * sb, struct inode * inode, uint32_t ino);
	return ext4_inode_init(inode->i_superblock, inode, inode->i_ino);
}

/**
 * ext4_write_inode - Write an inode to the filesystem
 * @inode: The inode to write
 * @wait: Whether to wait for I/O to complete
 *
 * Returns: 0 on success, negative error code on failure
 */
static int ext4_write_inode(struct inode* inode, int wait) {
	struct ext4_inode_ref inode_ref;
	struct ext4_fs* e_fs = inode->i_superblock->s_fs_info;
	struct ext4_sblock* e_sb = &e_fs->sb;
	int ret;

	if (!inode || !e_fs) return -EINVAL;

	/* Get the ext4 inode reference */
	ret = ext4_fs_get_inode_ref(e_fs, inode->i_ino, &inode_ref);
	if (ret != 0) return ret;

	/* Update the ext4 inode from VFS inode */
	ext4_inode_set_mode(e_sb, inode_ref.inode, inode->i_mode);
	ext4_inode_set_uid(inode_ref.inode, inode->i_uid);
	ext4_inode_set_gid(inode_ref.inode, inode->i_gid);
	ext4_inode_set_size(inode_ref.inode, inode->i_size);

	ext4_inode_set_access_time(inode_ref.inode, timespec64_to_ext4_timestamp(&inode->i_atime));
	ext4_inode_set_modif_time(inode_ref.inode, timespec64_to_ext4_timestamp(&inode->i_mtime));
	ext4_inode_set_change_inode_time(inode_ref.inode, timespec64_to_ext4_timestamp(&inode->i_ctime));
	ext4_inode_set_links_cnt(inode_ref.inode, inode->i_nlink);

	/* Sync the inode if requested */
	if (wait) { ret = ext4_fs_sync_inode(e_fs, inode_ref.inode); }

	/* Release the ext4 inode reference */
	ext4_fs_put_inode_ref(&inode_ref);

	return ret;
}

/**
 * ext4_put_super - Release a superblock
 * @sb: The superblock to release
 */
static void ext4_put_super(struct superblock* sb) {
	struct ext4_fs* fs = sb->s_fs_info;

	if (!fs) return;

	/* Sync the filesystem */
	ext4_fs_sync(fs);

	/* Unmount the filesystem */
	ext4_fs_fini(fs);

	/* Free the ext4_fs structure */
	kfree(fs);
	sb->s_fs_info = NULL;
}

/**
 * ext4_statfs - Get filesystem statistics
 * @sb: The superblock
 * @stat: Structure to fill with filesystem statistics
 *
 * Returns: 0 on success, negative error code on failure
 * 
 * 取自lwext4库中 ext4_mount_point_stats
 * 
 */
static int ext4_statfs(struct superblock* sb, struct statfs* stats) {
	CHECK_PTR(sb, -EINVAL);
	CHECK_PTR(stat, -EINVAL);

	struct ext4_fs* e_fs = sb->s_fs_info;
	CHECK_PTR(e_fs, -EINVAL);

	struct ext4_sblock* e_sb = &e_fs->sb;
	CHECK_PTR(e_sb, -EINVAL);

	ext4_lock();
	stats->f_type = ext4_get16(e_sb, magic);
	stats->f_bsize = ext4_sb_get_block_size(e_sb);
	stats->f_blocks = ext4_sb_get_blocks_cnt(e_sb);
	stats->f_bfree = ext4_sb_get_free_blocks_cnt(e_sb);
	stats->f_bavail = ext4_sb_get_free_blocks_cnt(e_sb);
	stats->f_files = ext4_get32(e_sb, inodes_count);
	stats->f_ffree = ext4_get32(e_sb, free_inodes_count);
	make_fsid_from_uuid(e_sb->uuid, &stats->f_fsid);
	stats->f_namelen = EXT4_DIRECTORY_FILENAME_LEN;
	stats->f_frsize = ext4_sb_get_block_size(e_sb);
	stats->f_flags = ST_NOSUID | ST_NODEV | ST_NOEXEC;
	ext4_unlock();

	return 0;
}

/**
 * ext4_sync_fs - Sync the filesystem
 * @sb: The superblock
 * @wait: Whether to wait for I/O to complete
 *
 * Returns: 0 on success, negative error code on failure
 */
static int ext4_sync_fs(struct superblock* sb, int wait) {
	CHECK_PTR(sb, -EINVAL);
	struct ext4_fs* fs = sb->s_fs_info;

	if (!fs) return -EINVAL;

	/* Sync the filesystem */
	return ext4_fs_sync(fs);
}

/**
 * ext4_fill_super - Fill a superblock with filesystem information
 * @sb: The superblock to fill
 * @data: Mount options
 * @silent: Whether to suppress error messages
 *
 * Returns: 0 on success, negative error code on failure
 */
int ext4_fill_super(struct superblock* sb, void* data, int silent) {
	/* Allocate and initialize the ext4_fs structure */
	struct ext4_fs* e_fs = kmalloc(sizeof(struct ext4_fs));
	if (!e_fs) return -ENOMEM;
	memset(e_fs, 0, sizeof(struct ext4_fs));

	/* Get the block device from the superblock */
	struct ext4_blockdev* bdev = sb->s_bdev;
	if (!bdev) {
		kfree(e_fs);
		return -EINVAL;
	}

	/* Initialize the filesystem */
	int ret = ext4_fs_init(e_fs, bdev, sb->s_flags & MS_RDONLY);
	if (ret != 0) {
		kfree(e_fs);
		return ret;
	}

	/* Set up the superblock */
	sb->s_blocksize = ext4_sb_get_block_size(&e_fs->sb);
	sb->s_blocksize_bits = ffs(sb->s_blocksize) - 1;
	sb->s_magic = EXT4_SUPERBLOCK_MAGIC;
	sb->s_operations = &ext4_superblock_operations;
	sb->s_fs_info = e_fs;
	sb->s_max_links = EXT4_LINK_MAX;

	/* Read the root inode */
	//sb->s_root = dentry_get(e_fs->sb.s_root);

	return 0;
}

/**
 * ext4_mount - Mount an ext4 filesystem
 * @fs_type: The filesystem type
 * @flags: Mount flags
 * @dev_name: Device name
 * @data: Mount options
 *
 * Returns: The mounted superblock or an error pointer
 */
struct superblock* ext4_mount(struct fstype* fs_type, int flags, const char* dev_name, void* data) {
	struct superblock* sb;
	int ret;

	/* Create a new superblock */
	sb = fstype_acquireSuperblock(fs_type, NULL, ext4_fill_super, flags, data);
	if (IS_ERR(sb)) return sb;

	/* Set the device name */
	if (dev_name && *dev_name) sb->s_dev_name = kstrdup(dev_name, 0);

	return sb;
}

/* Filesystem type registration structure */
static struct fstype ext4_fs_type = {
    .fs_name = "ext4",
    .fs_flags = FS_REQUIRES_DEV,
    .fs_mount = ext4_mount,
    .fs_kill_sb = kill_block_super,
};

/**
 * ext4_init - Initialize the ext4 filesystem driver
 *
 * Returns: 0 on success, negative error code on failure
 */
int ext4_init(void) { return register_filesystem(&ext4_fs_type); }