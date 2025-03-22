#ifndef _FSTYPE_H
#define _FSTYPE_H

#include <kernel/types.h>
#include <util/list.h>
#include <util/spinlock.h>

/* File system types */
struct fsType {
	const char* fs_name;
	int fs_flags;

	/* Fill in a superblock */
	int (*fs_fill_sb)(struct superblock* sb, void* data, int silent);
	struct superblock* (*fs_mount_sb)(struct fsType*, int, const char* mount_path, void*);
	void (*fs_kill_sb)(struct superblock*);

	/* Inside fsType structure */
	struct list_node fs_node_gfslist; /* Node for linking into global filesystem list */
	                                 // spinlock_t fs_fslist_node_lock;

	struct list_head fs_list_sb;
	spinlock_t fs_list_s_lock;
};



int fsType_register_all(void);
int fsType_register(struct fsType*);
int fsType_unregister(struct fsType*);

struct superblock* fsType_acquireSuperblock(struct fsType* type, dev_t dev_id, void* fs_data);
struct superblock* fsType_createMount(struct fsType* type, int flags, const char* dev_name, void* data);
struct fsType* fsType_lookup(const char* name);


#endif