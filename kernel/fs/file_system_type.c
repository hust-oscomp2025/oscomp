#include <errno.h>
#include <kernel/fs/file_system_type.h>
#include <kernel/fs/vfs.h>
#include <spike_interface/spike_utils.h>
#include <util/list.h>
#include <util/spinlock.h>
#include <util/string.h>

/* Head of the filesystem types list */
static struct list_head file_systems_list;
static spinlock_t file_systems_lock;


/**
 * Register built-in filesystem types
 * called by vfs_init
 */
int register_filesystem_types(void) {
	INIT_LIST_HEAD(&file_systems_list);
	spinlock_init(&file_systems_lock);
  int err;
  /* Register ramfs - our initial root filesystem */
  extern struct file_system_type ramfs_fs_type;
  err = register_filesystem(&ramfs_fs_type);
  if (err < 0)
    return err;

  /* Register other built-in filesystems */
  extern struct file_system_type hostfs_fs_type;
  err = register_filesystem(&hostfs_fs_type);
  if (err < 0)
    return err;

  return 0;
}

/**
 * register_filesystem - Register a new filesystem type
 * @fs: The filesystem type structure to register
 *
 * Adds a filesystem to the kernel's list of filesystems that can be mounted.
 * Returns 0 on success, error code on failure.
 */
int register_filesystem(struct file_system_type *fs) {
  struct file_system_type* p;

  if (!fs || !fs->name)
    return -EINVAL;

  /* Initialize filesystem type */
	INIT_LIST_HEAD(&fs->list_node);

  /* Acquire lock for list manipulation */
  spinlock_init(&file_systems_lock);

  /* Check if filesystem already registered */
	list_for_each_entry(p, &file_systems_list, list_node) {
		if (strcmp(p->name, fs->name) == 0) {
			/* Already registered */
			release_spinlock(&file_systems_lock);
			sprint("VFS: Filesystem %s already registered\n", fs->name);
			return -EBUSY;
		}
	}

  /* Add filesystem to the list (at the beginning for simplicity) */
	list_add(&fs->list_node, &file_systems_list);

  spinlock_unlock(&file_systems_lock);
  sprint("VFS: Registered filesystem %s\n", fs->name);
  return 0;
}

/**
 * unregister_filesystem - Remove a filesystem type from the kernel's list
 * @fs: The filesystem type structure to unregister
 *
 * Removes a filesystem from the kernel's list of available filesystems.
 * Returns 0 on success, error code on failure.
 */
int unregister_filesystem(struct file_system_type *fs) {
  struct file_system_type *p;

  if (!fs || !fs->name)
    return -EINVAL;

  /* Acquire lock for list manipulation */
  acquire_spinlock(&file_systems_lock);

  /* Find filesystem in the list */
	list_for_each_entry(p, &file_systems_list, list_node) {
		if (p == fs) {
			/* Found it - remove from the list */
			list_del(&p->list_node);
			release_spinlock(&file_systems_lock);
			sprint("VFS: Unregistered filesystem %s\n", p->name);
			return 0;
		}
	}

  release_spinlock(&file_systems_lock);
  sprint("VFS: Filesystem %s not registered\n", fs->name);
  return -ENOENT;
}

/**
 * get_fs_type - Find a filesystem type by name
 * @name: The filesystem name to find
 *
 * Searches the list of registered filesystems for one with the given name.
 * Returns a pointer to the filesystem type structure or NULL if not found.
 */
struct file_system_type *get_fs_type(const char *name) {
  struct file_system_type *fs;

  if (!name)
    return NULL;

  acquire_spinlock(&file_systems_lock);

	list_for_each_entry(fs, &file_systems_list, list_node) {
		if (strcmp(fs->name, name) == 0) {
			release_spinlock(&file_systems_lock);
			return fs;
		}
	}

  release_spinlock(&file_systems_lock);
  return NULL;
}

