#ifndef _NAMESPACE_H
#define _NAMESPACE_H

#include <kernel/types.h>
#include <util/atomic.h>
#include <util/list.h>

/* Forward declarations */
struct superblock;
struct dentry;
struct path;

/* Mount flags */
#define MNT_RDONLY 1              /* Mount read-only */
#define MNT_NOSUID 2              /* Ignore suid and sgid bits */
#define MNT_NODEV 4               /* Disallow access to device special files */
#define MNT_NOEXEC 8              /* Disallow program execution */
#define MNT_RELATIME (1 << 21)    /* Update atime relative to mtime/ctime */
#define MNT_STRICTATIME (1 << 29) /* Always perform atime updates */

/**
 * Mount namespace structure
 *
 * A mount namespace contains an isolated view of the filesystem hierarchy.
 * Different processes can have different mount namespaces, allowing
 * containerization and isolation.
 */
struct mnt_namespace {
	/* Root of the mount tree for this namespace */
	struct vfsmount* root;

	/* All mounts in this namespace */
	struct list_head mount_list;

	/* Count of mounts in this namespace */
	int mount_count;

	/* Reference counting */
	atomic_t count;

	/* Owner info */
	uid_t owner;

	/* Protection */
	spinlock_t lock;
};



/* Mount registry management */
struct mnt_namespace* create_namespace(struct mnt_namespace* parent);
inline struct mnt_namespace* grab_namespace(struct mnt_namespace* ns);
void put_namespace(struct mnt_namespace* ns);



#endif /* _NAMESPACE_H */

// [rootfs mount]
// ├── parent: NULL
// ├── children: [/home mount], [/mnt/cdrom mount]
// │
// ├── [/home mount]
// │   ├── parent: [rootfs mount]
// │   └── children: [/home/user/data mount]
// │       │
// │       └── [/home/user/data mount]
// │           ├── parent: [/home mount]
// │           └── children: []
// │
// └── [/mnt/cdrom mount]
//     ├── parent: [rootfs mount]
//     └── children: []