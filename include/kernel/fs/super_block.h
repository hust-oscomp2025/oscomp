#ifndef SUPER_BLOCK_H
#define SUPER_BLOCK_H
#include <kernel/fs/vfs.h>
#include <kernel/types.h>

// general-purpose super_block structure
struct super_block {
  int magic;              // magic number of the file system
  int size;               // size of file system image (blocks)
  int nblocks;            // number of data blocks
  int ninodes;            // number of inodes.
  struct dentry *s_root;  // root dentry of inode
  struct device *s_dev;   // device of the superblock
  void *s_fs_info;        // filesystem-specific info. for rfs, it points bitmap
};




#endif