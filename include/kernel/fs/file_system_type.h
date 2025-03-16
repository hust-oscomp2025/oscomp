#ifndef _FILE_SYSTEM_TYPE_H
#define _FILE_SYSTEM_TYPE_H

#include <kernel/types.h>

/* File system types */
struct file_system_type {
  const char *name;
  int fs_flags;

  /* Fill in a superblock */
  int (*fill_super)(struct super_block *sb, void *data, int silent);
  struct super_block *(*mount)(struct file_system_type *, int, const char *,
                               void *);
  void (*kill_sb)(struct super_block *);


	
  struct list_head list_node;
};

int register_filesystem_types(void);

int register_filesystem(struct file_system_type *);
int unregister_filesystem(struct file_system_type *);
struct file_system_type *get_fs_type(const char *name);

#endif