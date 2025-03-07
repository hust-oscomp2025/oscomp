#ifndef _FILE_H
#define _FILE_H
#include <kernel/vfs.h>
#include <kernel/types.h>
#include <kernel/fmode.h>
#include <spike_interface/atomic.h>

typedef struct dentry dentry_t;


struct file *alloc_vfs_file(dentry_t *file_dentry, int readable,
	int writable, int offset);



struct path {
	struct dentry *mnt_root;	/* root of the mounted tree */
	struct super_block *mnt_sb;	/* pointer to superblock */
	struct dentry *dentry;
};

// data structure of an openned file
struct file {
	struct path		f_path;
  struct dentry *f_dentry;
	//const struct file_operations	*f_op;
	spinlock_t		f_lock;
	fmode_t f_mode;
	//struct mutex		f_pos_lock;
  int f_pos;
	// struct file_ra_state	f_ra;
	// struct address_space	*f_mapping;
};

#endif