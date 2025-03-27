#ifndef _HOSTFS_H_
#define _HOSTFS_H_
#include <kernel/vfs.h>

#define HOSTFS_TYPE 1

// root directory
#define H_ROOT_DIR "./hostfs"

// hostfs utility functin declarations
int32 register_hostfs();
struct device *init_host_device(char *name);
void get_path_string(char *path, struct dentry *dentry);
struct inode *hostfs_alloc_vinode(struct superblock *sb);
int32 hostfs_write_back_vinode(struct inode *vinode);
int32 hostfs_update_vinode(struct inode *vinode);

// hostfs interface function declarations
ssize_t hostfs_read(struct inode *f_inode, char *r_buf, ssize_t len,
                    int32 *offset);
ssize_t hostfs_write(struct inode *f_inode, const char *w_buf, ssize_t len,
                     int32 *offset);
struct inode *hostfs_lookup(struct inode *parent, struct dentry *sub_dentry);
struct inode *hostfs_create(struct inode *parent, struct dentry *sub_dentry);
int32 hostfs_lseek(struct inode *f_inode, ssize_t new_offset, int32 whence,
                  int32 *offset);
int32 hostfs_link(struct inode *parent, struct dentry *sub_dentry, struct inode *link_node);
int32 hostfs_unlink(struct inode *parent, struct dentry *sub_dentry, struct inode *unlink_node);
int32 hostfs_hook_open(struct inode *f_inode, struct dentry *dentry);
int32 hostfs_hook_close(struct inode *f_inode, struct dentry *dentry);
int32 hostfs_readdir(struct inode *dir_vinode, struct dir *dir, int32 *offset);
struct inode *hostfs_mkdir(struct inode *parent, struct dentry *sub_dentry);
struct superblock *hostfs_get_superblock(struct device *dev);

extern const struct inode_operations hostfs_node_ops;

#endif
