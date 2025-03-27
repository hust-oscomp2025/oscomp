#include <kernel/vfs.h>


struct kiocb;
/*
 * Inode operations
 */
struct inode_operations {
	/* File operations */
	struct dentry* (*lookup)(struct inode*, struct dentry*, uint32);
	struct inode* (*create)(struct inode*, struct dentry*, fmode_t, bool);
	int32 (*link)(struct dentry*, struct inode*, struct dentry*);
	int32 (*unlink)(struct inode*, struct dentry*);
	int32 (*symlink)(struct inode*, struct dentry*, const char*);
	int32 (*mkdir)(struct inode*, struct dentry*, fmode_t);
	int32 (*rmdir)(struct inode*, struct dentry*);
	int32 (*mknod)(struct inode*, struct dentry*, fmode_t, dev_t);
	int32 (*rename)(struct inode*, struct dentry*, struct inode*, struct dentry*, uint32);

	/* Extended attribute operations */
	int32 (*setxattr)(struct dentry*, const char*, const void*, size_t, int32);
	ssize_t (*getxattr)(struct dentry*, const char*, void*, size_t);
	ssize_t (*listxattr)(struct dentry*, char*, size_t);
	int32 (*removexattr)(struct dentry*, const char*);

	/* Special file operations */
	int32 (*readlink)(struct dentry*, char*, int32);
	int32 (*get_link)(struct dentry*, struct inode*, struct path*);
	int32 (*permission)(struct inode*, int32);
	int32 (*get_acl)(struct inode*, int32);
    //int32 (*set_acl)(struct inode*, struct posix_acl*, int32);
	int32 (*setattr)(struct dentry*, struct iattr*);
	int32 (*getattr)(const struct path*, struct kstat*, uint32, uint32);
	int32 (*fiemap)(struct inode*, struct fiemap_extent_info*, uint64, uint64);

	/* Block operations */
	int32 (*get_block)(struct inode*, sector_t, struct buffer_head*, int32 create);
	sector_t (*bmap)(struct inode*, sector_t);
	void (*truncate_blocks)(struct inode*, loff_t size);

	/* Direct I/O support */
	int32 (*direct_IO)(struct kiocb*, struct io_vector_iterator*);
    // ACL operations
    // Memory mapping operations
    vm_fault_t (*page_fault)(struct vm_area_struct *, struct vm_fault *);
    uint64 (*get_unmapped_area)(struct file *, uint64, uint64, uint64, uint64);

    // POSIX specific operations
    int32 (*atomic_open)(struct inode *, struct dentry *, struct file *, unsigned open_flag, umode_t create_mode);
    int32 (*tmpfile)(struct inode *, struct dentry *, umode_t);
    //int32 (*dentry_open)(struct dentry *, struct file *, const struct cred *);

};
