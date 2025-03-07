#ifndef _RFS_H_
#define _RFS_H_

#include <kernel/ramdev.h>
#include <kernel/riscv.h>
#include <kernel/types.h>
#include <kernel/vfs.h>

#define RFS_TYPE 0
#define RFS_MAGIC 0xBEAF
#define RFS_BLKSIZE PGSIZE
#define RFS_INODESIZE 128  // block size must be divisible by this value
#define RFS_MAX_INODE_BLKNUM 10
#define RFS_MAX_FILE_NAME_LEN 28
#define RFS_DIRECT_BLKNUM DIRECT_BLKNUM

// rfs block offset
#define RFS_BLK_OFFSET_SUPER 0
#define RFS_BLK_OFFSET_INODE 1
#define RFS_BLK_OFFSET_BITMAP 11
#define RFS_BLK_OFFSET_FREE 12

// dinode type
#define R_FILE S_IFREG
#define R_DIR S_IFDIR
#define R_FREE 2

// file system super block
struct rfs_superblock {
  int magic;    // magic number of the
  int size;     // size of file system image (blocks)
  int nblocks;  // number of data blocks
  int ninodes;  // number of inodes.
};

// disk inode
struct rfs_dinode {
  int size;                      // size of the file (in bytes)
  int type;                      // one of R_FREE, R_FILE, R_DIR
  int nlinks;                    // number of hard links to this file
  int blocks;                    // number of blocks
  int addrs[RFS_DIRECT_BLKNUM];  // direct blocks
};

// directory entry
struct rfs_direntry {
  int inum;                          // inode number
  char name[RFS_MAX_FILE_NAME_LEN];  // file name
};

// directory memory cache (used by opendir/readdir/closedir)
struct rfs_dir_cache {
  int block_count;
  struct rfs_direntry *dir_base_addr;
};

// rfs utility functin declarations
int register_rfs();
int rfs_format_dev(struct device *dev);



struct rfs_dinode *rfs_read_dinode(struct rfs_device *rdev, int n_inode);
int rfs_write_dinode(struct rfs_device *rdev, const struct rfs_dinode *dinode,
                     int n_inode);
int rfs_alloc_block(struct super_block *sb);
int rfs_free_block(struct super_block *sb, int block_num);
int rfs_add_direntry(struct inode *dir, const char *name, int inum);

struct inode *rfs_alloc_vinode(struct super_block *sb);
int rfs_write_back_vinode(struct inode *vinode);
int rfs_update_vinode(struct inode *vinode);

// rfs interface function declarations
ssize_t rfs_read(struct inode *f_inode, char *r_buf, ssize_t len, int *offset);
ssize_t rfs_write(struct inode *f_inode, const char *w_buf, ssize_t len,
                  int *offset);
struct inode *rfs_lookup(struct inode *parent, struct dentry *sub_dentry);
struct inode *rfs_create(struct inode *parent, struct dentry *sub_dentry);
int rfs_lseek(struct inode *f_inode, ssize_t new_offset, int whence, int *offset);
int rfs_disk_stat(struct inode *vinode, struct istat *istat);
int rfs_link(struct inode *parent, struct dentry *sub_dentry, struct inode *link_node);
int rfs_unlink(struct inode *parent, struct dentry *sub_dentry, struct inode *unlink_vinode);

int rfs_hook_opendir(struct inode *dir_vinode, struct dentry *dentry);
int rfs_hook_closedir(struct inode *dir_vinode, struct dentry *dentry);
int rfs_readdir(struct inode *dir_vinode, struct dir *dir, int *offset);
struct inode *rfs_mkdir(struct inode *parent, struct dentry *sub_dentry);

struct super_block *rfs_get_superblock(struct device *dev);

extern const struct vinode_ops rfs_i_ops;

#endif
