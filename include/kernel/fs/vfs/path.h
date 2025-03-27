#ifndef _PATH_H
#define _PATH_H

#include <kernel/types.h>

struct vfsmount;
struct dentry;
/**
 * File path representation
 */
struct path {
  struct vfsmount* mnt;  /* Mount information */
  struct dentry* dentry; /* Directory entry */
};

/* Path lookup and traversal */
int32 path_create(const char* name, uint32 flags, struct path* result);
//int32 kern_path_qstr(const struct qstr *name, uint32 flags, struct path *result);
void path_destroy(struct path* path);

int32 filename_lookup(int32 dfd, const char* name, uint32 flags,
                    struct path* path, struct path* started);

/* Lookup flags */
#define LOOKUP_FOLLOW 0x0001    /* Follow terminal symlinks */
#define LOOKUP_DIRECTORY 0x0002 /* Want only directories */
#define LOOKUP_AUTOMOUNT 0x0004 /* Follow automounts */
#define LOOKUP_PARENT 0x0010    /* Find parent directory */
#define LOOKUP_REVAL 0x0020     /* Check if dentries are still valid */
#define LOOKUP_RCU 0x0080       /* RCU pathwalk mode */
#define LOOKUP_OPEN 0x0100      /* Open in progress */
#define LOOKUP_CREATE 0x0200    /* Create in progress */

#endif