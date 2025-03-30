#include <kernel/mmu.h>
#include <kernel/time.h>
#include <kernel/util.h>
#include <kernel/vfs.h>

/**
 * static_ramfs_fill_super - Fill ramfs superblock
 * @type: Filesystem type
 * @sb: Superblock to fill
 * @data: Mount options
 * @silent: Whether to print error messages
 *
 * Initializes a superblock for ramfs
 */
static int32 static_ramfs_fill_super(struct fstype* type, struct superblock* sb, void* data, int32 silent) {
	struct inode* root_inode;
	struct dentry* root_dentry;

	// Set filesystem-specific operations
	sb->s_blocksize = PAGE_SIZE;
	sb->s_blocksize_bits = PAGE_SHIFT;
	sb->s_magic = 0x858458f6; // Ramfs magic number
	sb->s_flags |= MS_NODEV | MS_NOSUID;
	sb->s_file_maxbytes = UINT64_MAX;
	sb->s_time_granularity = 1;

	// Create root inode
	root_inode = kmalloc(sizeof(struct inode));
	if (!root_inode) return -ENOMEM;

	memset(root_inode, 0, sizeof(struct inode));
	root_inode->i_ino = 1; // Root inode number is 1
	root_inode->i_mode = S_IFDIR | 0755;
	root_inode->i_size = 0;
	root_inode->i_blocks = 0;
	root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime = current_time(sb);
	atomic_set(&root_inode->i_refcount, 1);
	root_inode->i_superblock = sb;

	// Set static inode operations for directories
	static struct inode_operations ramfs_dir_inode_operations = {
	    // .lookup = simple_lookup,
	    // .create = simple_create,
	    // .mkdir = simple_mkdir,
	    // .rmdir = simple_rmdir,
	    // .unlink = simple_unlink,
	};

	static struct file_operations ramfs_dir_operations = {
	    // .read = generic_read_dir,
	    // .iterate = simple_readdir,
	};

	root_inode->i_op = &ramfs_dir_inode_operations;
	root_inode->i_fop = &ramfs_dir_operations;

	// Create root dentry
	root_dentry = kmalloc(sizeof(struct dentry));
	if (!root_dentry) {
		kfree(root_inode);
		return -ENOMEM;
	}

	memset(root_dentry, 0, sizeof(struct dentry));
	root_dentry->d_name->name = kstrdup("/", 0);
	// root_dentry->d_name.name = "/";
	root_dentry->d_name->len = 1;
	root_dentry->d_name->hash = full_name_hash("/", 1);
	root_dentry->d_inode = root_inode;
	root_dentry->d_superblock = sb;
	atomic_set(&root_dentry->d_refcount, 1);
	INIT_LIST_HEAD(&root_dentry->d_childList);

	sb->s_root = root_dentry;

	return 0;
}

/**
 * static_ramfs_mount - Mount a ramfs filesystem
 * @type: Filesystem type
 * @flags: Mount flags
 * @dev_name: Device name (unused for ramfs)
 * @data: Mount options
 *
 * Creates and returns a new superblock for ramfs
 */
static struct superblock* static_ramfs_mount(struct fstype* type, int32 flags, dev_t dev_id, void* data) {
	struct superblock* sb;
	int32 error;

	// Allocate superblock
	sb = kmalloc(sizeof(struct superblock));
	if (!sb) return ERR_PTR(-ENOMEM);

	memset(sb, 0, sizeof(struct superblock));

	// Initialize superblock lists
	INIT_LIST_HEAD(&sb->s_list_mounts);
	spinlock_init(&sb->s_list_mounts_lock);

	// Fill superblock
	error = static_ramfs_fill_super(type, sb, data, flags & MS_SILENT);
	if (error) {
		kfree(sb);
		return ERR_PTR(error);
	}

	return sb;
}

/**
 * static_ramfs_kill_sb - Kill a ramfs superblock
 * @sb: Superblock to kill
 *
 * Cleans up resources when unmounting a ramfs
 */
static void static_ramfs_kill_sb(struct superblock* sb) {
	// Free the superblock root (would need to recursively free all dentries)
	if (sb->s_root) {
		// In a full implementation, recursively free all dentries and inodes
		// For this minimal example, we just release the root
		kfree(sb->s_root->d_inode);
		kfree(sb->s_root);
	}

	kfree(sb);
}

/**
 * Static definition of ramfs filesystem type
 */
static struct fstype ramfs_fs_type = {
    .fs_name = "ramfs",
    .fs_flags = 0,
    .fs_mount = static_ramfs_mount,
    .fs_kill_sb = static_ramfs_kill_sb,
    .fs_capabilities = 0,
};

int32 init_ramfs() { return fstype_register(&ramfs_fs_type); }