#include <kernel/fs/vfs/vfs.h>



/*
 * Inode operations
 */
struct inode_operations {
	/* File operations */
	struct dentry* (*lookup)(struct inode*, struct dentry*, unsigned int);
	struct inode* (*create)(struct inode*, struct dentry*, fmode_t, bool);
	int (*link)(struct dentry*, struct inode*, struct dentry*);
	int (*unlink)(struct inode*, struct dentry*);
	int (*symlink)(struct inode*, struct dentry*, const char*);
	int (*mkdir)(struct inode*, struct dentry*, fmode_t);
	int (*rmdir)(struct inode*, struct dentry*);
	int (*mknod)(struct inode*, struct dentry*, fmode_t, dev_t);
	int (*rename)(struct inode*, struct dentry*, struct inode*, struct dentry*, unsigned int);

	/* Extended attribute operations */
	int (*setxattr)(struct dentry*, const char*, const void*, size_t, int);
	ssize_t (*getxattr)(struct dentry*, const char*, void*, size_t);
	ssize_t (*listxattr)(struct dentry*, char*, size_t);
	int (*removexattr)(struct dentry*, const char*);

	/* Special file operations */
	int (*readlink)(struct dentry*, char*, int);
	int (*get_link)(struct dentry*, struct inode*, struct path*);
	int (*permission)(struct inode*, int);
	int (*get_acl)(struct inode*, int);
	int (*setattr)(struct dentry*, struct iattr*);
	int (*getattr)(const struct path*, struct kstat*, unsigned int, unsigned int);
	int (*fiemap)(struct inode*, struct fiemap_extent_info*, uint64, uint64);

	/* Block operations */
	int (*get_block)(struct inode*, sector_t, struct buffer_head*, int create);
	sector_t (*bmap)(struct inode*, sector_t);
	void (*truncate_blocks)(struct inode*, loff_t size);

	/* Direct I/O support */
	int (*direct_IO)(struct kiocb*, struct io_vector_iterator*);
    // ACL operations
    struct posix_acl* (*get_acl)(struct inode*, int);
    int (*set_acl)(struct inode*, struct posix_acl*, int);
    // Memory mapping operations
    vm_fault_t (*page_fault)(struct vm_area_struct *, struct vm_fault *);
    unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned long, unsigned long, unsigned long);

    // POSIX specific operations
    int (*atomic_open)(struct inode *, struct dentry *, struct file *, unsigned open_flag, umode_t create_mode);
    int (*tmpfile)(struct inode *, struct dentry *, umode_t);
    int (*dentry_open)(struct dentry *, struct file *, const struct cred *);

};
