#ifndef _VFS_H_
#define _VFS_H_

#include <kernel/types.h>
#include <kernel/file.h>
#include <kernel/inode.h>
#include <kernel/super_block.h>

#define MAX_VFS_DEV 10            // the maximum number of vfs_dev_list
#define MAX_DENTRY_NAME_LEN 30    // the maximum length of dentry name
#define MAX_DEVICE_NAME_LEN 30    // the maximum length of device name
#define MAX_MOUNTS 10             // the maximum number of mounts
#define MAX_DENTRY_HASH_SIZE 100  // the maximum size of dentry hash table
#define MAX_PATH_LEN 30           // the maximum length of path
#define MAX_SUPPORTED_FS 10       // the maximum number of supported file systems



/**** vfs initialization function ****/
int vfs_init();

/**** vfs interfaces ****/

// device interfaces
struct super_block *vfs_mount(const char *dev_name, int mnt_type);

// file interfaces
struct file *vfs_open(const char *path, int flags);
ssize_t vfs_read(struct file *file, char *buf, size_t count);
ssize_t vfs_write(struct file *file, const char *buf, size_t count);
ssize_t vfs_lseek(struct file *file, ssize_t offset, int whence);
int vfs_stat(struct file *file, struct istat *istat);
int vfs_disk_stat(struct file *file, struct istat *istat);
int vfs_link(const char *oldpath, const char *newpath);
int vfs_unlink(const char *path);
int vfs_close(struct file *file);

// directory interfaces
struct file *vfs_opendir(const char *path);
int vfs_readdir(struct file *file, struct dir *dir);
int vfs_mkdir(const char *path);
int vfs_closedir(struct file *file);

/**** vfs abstract object types ****/
// system root direntry
extern struct dentry *vfs_root_dentry;

// vfs abstract dentry
typedef struct dentry {
  char name[MAX_DENTRY_NAME_LEN];
  int d_ref;
  struct inode *dentry_inode;
  struct dentry *parent;
  struct super_block *sb;
}dentry_t;


// dentry constructor and destructor
struct dentry *alloc_vfs_dentry(const char *name, struct inode *inode,
                            struct dentry *parent);
int free_vfs_dentry(struct dentry *dentry);

// ** dentry hash table **
extern struct hash_table dentry_hash_table;

// dentry hash table key type
struct dentry_key {
  struct dentry *parent;
  char *name;
};

// generic hash table method implementation
int dentry_hash_equal(void *key1, void *key2);
size_t dentry_hash_func(void *key);

// dentry hash table interface
struct dentry *hash_get_dentry(struct dentry *parent, char *name);
int hash_put_dentry(struct dentry *dentry);
int hash_erase_dentry(struct dentry *dentry);



// abstract device entry in vfs_dev_list
struct device {
  char dev_name[MAX_DEVICE_NAME_LEN];  // the name of the device
  int dev_id;  // the id of the device (the meaning of an id is interpreted by
               // the specific file system, all we need to know is that it is
               // a unique identifier)
  struct file_system_type *fs_type;  // the file system type in the device
};

// device list in vfs layer
extern struct device *vfs_dev_list[MAX_VFS_DEV];

// supported file system types
struct file_system_type {
  int type_num;  // the number of the file system type
  struct super_block *(*get_superblock)(struct device *dev);
};

extern struct file_system_type *fs_list[MAX_SUPPORTED_FS];

// other utility functions
struct inode *default_alloc_vinode(struct super_block *sb);
struct dentry *lookup_final_dentry(const char *path, struct dentry **parent,
                                   char *miss_name);
void get_base_name(const char *path, char *base_name);

#endif
