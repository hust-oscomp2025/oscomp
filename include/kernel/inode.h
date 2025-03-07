#ifndef INODE_H
#define INODE_H

#include <kernel/vfs.h>
#include <kernel/types.h>
#include <kernel/super_block.h>

#define DIRECT_BLKNUM 10          // the number of direct blocks

// vinode hash table
extern struct hash_table vinode_hash_table;


// generic hash table method implementation
int inode_hash_equal(void *key1, void *key2);
size_t inode_hash_func(void *key);

// vinode hash table interface
struct inode *hash_get_inode(struct super_block *sb, int inum);
int hash_put_inode(struct inode *vinode);
int hash_erase_inode(struct inode *vinode);

// abstract vfs inode
struct inode {
  int inum;                  // inode number of the disk inode
  int ref;                   // reference count
  int size;                  // size of the file (in bytes)
  int type;                  // one of S_IFREG, S_IFDIR
  int nlinks;                // number of hard links to this file
  int blocks;                // number of blocks
  int addrs[DIRECT_BLKNUM];  // direct blocks
  void *i_fs_info;           // filesystem-specific info (see s_fs_info)
  struct super_block *sb;          // super block of the vfs inode
  const struct inode_ops *i_ops;  // vfs inode operations
};

// vinode hash table key type
struct inode_key {
  int inum;
  struct super_block *sb;
};


struct inode_ops {
  // file operations
  ssize_t (*viop_read)(struct inode *node, char *buf, ssize_t len,
                       int *offset);
  ssize_t (*viop_write)(struct inode *node, const char *buf, ssize_t len,
                        int *offset);
  struct inode *(*viop_create)(struct inode *parent, struct dentry *sub_dentry);
  int (*viop_lseek)(struct inode *node, ssize_t new_off, int whence, int *off);
  int (*viop_disk_stat)(struct inode *node, struct istat *istat);
  int (*viop_link)(struct inode *parent, struct dentry *sub_dentry,
                   struct inode *link_node);
  int (*viop_unlink)(struct inode *parent, struct dentry *sub_dentry,
                     struct inode *unlink_node);
  struct inode *(*viop_lookup)(struct inode *parent,
                                struct dentry *sub_dentry);

  // directory operations
  int (*viop_readdir)(struct inode *dir_vinode, struct dir *dir, int *offset);
  struct inode *(*viop_mkdir)(struct inode *parent, struct dentry *sub_dentry);

  // write back inode to disk
  int (*viop_write_back_vinode)(struct inode *node);

  // hook functions
  // In the vfs layer, we do not assume that hook functions will do anything,
  // but simply call them (when they are defined) at the appropriate time.
  // Hook functions exist because the fs layer may need to do some additional
  // operations (such as allocating additional data structures) at some critical
  // times.
  int (*viop_hook_open)(struct inode *node, struct dentry *dentry);
  int (*viop_hook_close)(struct inode *node, struct dentry *dentry);
  int (*viop_hook_opendir)(struct inode *node, struct dentry *dentry);
  int (*viop_hook_closedir)(struct inode *node, struct dentry *dentry);
};

// inode operation interface
// the implementation depends on the vinode type and the specific file system

// virtual file system inode interfaces
#define viop_read(node, buf, len, offset)      (node->i_ops->viop_read(node, buf, len, offset))
#define viop_write(node, buf, len, offset)     (node->i_ops->viop_write(node, buf, len, offset))
#define viop_create(node, name)                (node->i_ops->viop_create(node, name))
#define viop_lseek(node, new_off, whence, off) (node->i_ops->viop_lseek(node, new_off, whence, off))
#define viop_disk_stat(node, istat)            (node->i_ops->viop_disk_stat(node, istat))
#define viop_link(node, name, link_node)       (node->i_ops->viop_link(node, name, link_node))
#define viop_unlink(node, name, unlink_node)   (node->i_ops->viop_unlink(node, name, unlink_node))
#define viop_lookup(parent, sub_dentry)        (parent->i_ops->viop_lookup(parent, sub_dentry))
#define viop_readdir(dir_vinode, dir, offset)  (dir_vinode->i_ops->viop_readdir(dir_vinode, dir, offset))
#define viop_mkdir(dir, sub_dentry)            (dir->i_ops->viop_mkdir(dir, sub_dentry))
#define viop_write_back_vinode(node)           (node->i_ops->viop_write_back_vinode(node))











#endif