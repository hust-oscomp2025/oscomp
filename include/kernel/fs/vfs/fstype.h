#ifndef _FSTYPE_H
#define _FSTYPE_H

#include <kernel/types.h>
#include <util/list.h>
#include <util/spinlock.h>

/* File system types */
struct fstype {
	const char* fs_name;
	int fs_flags;

	struct list_node fs_globalFsListNode;

	struct list_head fs_list_superblock;
	spinlock_t fs_list_superblock_lock;
	/* Add to struct fstype */
	unsigned long fs_capabilities; /* Capabilities like case sensitivity */

	// silent用来控制错误报告的方式，目前这个字段不使用
	int (*fs_op_fill_superblock)(struct fstype*,struct superblock* sb, void* data, int silent);
	struct superblock* (*fs_op_mount_superblock)(struct fstype*, int, const char* mount_path, void*);
	void (*fs_kill_sb)(struct superblock*);

	int (*fs_init)(void);  /* Called during registration */
	void (*fs_exit)(void); /* Called during unregistration */
};

#define fstype_mount_sb(type, flags, mount_path, data) ((type)->fs_op_mount_superblock ? (type)->fs_op_mount_superblock(type, flags, mount_path, data) : NULL)
#define fstype_kill_sb(type, sb) ((type)->fs_kill_sb ? (type)->fs_kill_sb(sb) : (void)0)
#define fstype_init(type) ((type)->fs_init ? (type)->fs_init() : 0)
#define fstype_exit(type) ((type)->fs_exit ? (type)->fs_exit() : (void)0)

//int parse_mount_options(const char* options, struct fs_mount_context* parsed);

int fstype_register_all(void);
int fstype_register(struct fstype*);
int fstype_unregister(struct fstype*);

struct superblock* fstype_acquireSuperblock(struct fstype* type, dev_t dev_id, void* fs_data);
struct superblock* fstype_handleMount(struct fstype* type, int flags, const char* dev_name, void* data);
int fstype_fill_sb(struct fstype* type, struct superblock* sb, void* data, int flags);
struct fstype* fstype_lookup(const char* name);

/* Filesystem type flags */
#define FS_REQUIRES_DEV 0x01       /* Filesystem requires a block device */
#define FS_BINARY_MOUNTDATA 0x02   /* Binary mount data instead of text */
#define FS_HAS_SUBTYPE 0x04        /* Subtype field valid */
#define FS_USERNS_MOUNT 0x08       /* Can be mounted in userns */
#define FS_RENAME_DOES_D_MOVE 0x20 /* FS will handle d_move in rename */

/* Capability flags */
#define FS_CAP_CASE_INSENSITIVE 1
#define FS_CAP_ATOMIC_RENAME 2

const char* fstype_error_string(int error_code);

#endif