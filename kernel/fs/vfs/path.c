#include <errno.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/fd_struct.h>
#include <kernel/fs/fs_struct.h>
#include <kernel/fs/inode.h>
#include <kernel/fs/namespace.h> // Add this include
#include <kernel/fs/super_block.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/path.h>
#include <kernel/sched/process.h>
#include <kernel/sched/sched.h>
#include <spike_interface/spike_utils.h>
#include <util/string.h>

static int vfs_path_lookup(struct dentry* base_dentry,
                           struct vfsmount* base_mnt, const char* path_str,
                           unsigned int flags, struct path* result);

/**
 * kern_path - Look up a path from the current working directory
 * @name: Path to look up
 * @flags: Lookup flags
 * @result: Result path
 *
 * This is a wrapper around vfs_path_lookup that uses the current
 * working directory as the starting point.
 */
int kern_path(const char* name, unsigned int flags, struct path* result) {
  int error;
  struct task_struct* current;
  struct dentry* start_dentry;
  struct vfsmount* start_mnt;

  /* Get current working directory (simplified) */
  start_dentry = CURRENT->fs->pwd.dentry;
  start_mnt = CURRENT->fs->pwd.mnt;

  /* Perform the lookup */
  error = vfs_path_lookup(start_dentry, start_mnt, name, flags, result);
  return error;
}

// Add this function to support qstr path lookups

/**
 * kern_path_qstr - Look up a path from qstr
 * @name: Path as qstr
 * @flags: Lookup flags
 * @result: Result path
 */
int kern_path_qstr(const struct qstr *name, unsigned int flags, struct path *result)
{
    /* For the initial implementation, convert to string and use existing kern_path */
    int ret;
    char *path_str;
    
    if (!name || !result)
        return -EINVAL;
        
    /* Convert qstr to char* */
    path_str = kmalloc(name->len + 1);
    if (!path_str)
        return -ENOMEM;
        
    memcpy(path_str, name->name, name->len);
    path_str[name->len] = '\0';
    
    /* Use existing path lookup */
    ret = kern_path(path_str, flags, result);
    
    kfree(path_str);
    return ret;
}



/**
 * put_path - Release a reference to a path
 * @path: Path to release
 *
 * Decrements the reference counts for both the dentry and vfsmount
 * components of a path structure.
 */
void put_path(struct path* path) {
  if (!path)
    return;

  /* Release dentry reference */
  if (path->dentry)
    put_dentry(path->dentry);

  /* Release mount reference */
  if (path->mnt)
    put_mount(path->mnt);

  /* Clear the path structure */
  path->dentry = NULL;
  path->mnt = NULL;
}

/**
 * filename_lookup - Look up a filename relative to a directory file descriptor
 * @dfd: Directory file descriptor (or AT_FDCWD for current working directory)
 * @name: Filename to look up (simple string)
 * @flags: Lookup flags
 * @path: Output path result
 * @started: Output path indicating starting directory (can be NULL)
 *
 * This function handles looking up a filename relative to a directory
 * file descriptor, supporting the *at() family of system calls.
 *
 * Returns 0 on success, negative error code on failure.
 */
int filename_lookup(int dfd, const char* name, unsigned int flags,
                    struct path* path, struct path* started) {
  struct task_struct* current;
  struct dentry* start_dentry;
  struct vfsmount* start_mnt;
  int error;

  /* Validate parameters */
  if (!name || !path)
    return -EINVAL;

  /* Check for absolute path */
  if (name[0] == '/') {
    /* Absolute path - always starts at root directory */
    current = CURRENT;
    start_dentry = current->fs->root.dentry;
    start_mnt = current->fs->root.mnt;
  } else {
    /* Relative path - get the starting directory */
    if (dfd == AT_FDCWD) {
      /* Use current working directory */
      current = CURRENT;
      start_dentry = current->fs->pwd.dentry;
      start_mnt = current->fs->pwd.mnt;
    } else {
      /* Use the directory referenced by the file descriptor */
      struct file* file = get_file(dfd, CURRENT);
      if (!file)
        return -EBADF;

      /* Check if it's a directory */
      if (!S_ISDIR(file->f_inode->i_mode)) {
        put_file(file, CURRENT);
        return -ENOTDIR;
      }

      start_dentry = file->f_path.dentry;
      start_mnt = file->f_path.mnt;

      /* Take reference to starting path components */
      start_dentry = get_dentry(start_dentry);
      if (start_mnt)
        get_mount(start_mnt);

      put_file(file, CURRENT);
    }
  }

  /* Save the starting path if requested */
  if (started) {
    started->dentry = get_dentry(start_dentry);
    started->mnt = start_mnt ? get_mount(start_mnt) : NULL;
  }

  /* Do the actual lookup */
  error = vfs_path_lookup(start_dentry, start_mnt, name, flags, path);

  /* Release references to starting directory */
  put_dentry(start_dentry);
  if (start_mnt)
    put_mount(start_mnt);

  return error;
}

/**
 * vfs_path_lookup - Look up a path relative to a dentry/mount pair
 * @base_dentry: Starting dentry
 * @base_mnt: Starting vfsmount
 * @path_str: Path string to look up
 * @flags: Lookup flags (LOOKUP_*)
 * @result: Result path (output)
 *
 * This function resolves a path string to a dentry/vfsmount pair.
 *
 * Returns 0 on success, negative error code on failure.
 */
static int vfs_path_lookup(struct dentry* base_dentry,
                           struct vfsmount* base_mnt, const char* path_str,
                           unsigned int flags, struct path* result) {
  struct nameidata nd;
  struct dentry* dentry;
  struct inode* inode;
  char* component;
  char* path_copy;
  char* next_slash;
  int len;

  /* Validate parameters */
  if (!base_dentry || !path_str || !result)
    return -EINVAL;

  /* Update mount point lookup in vfs_path_lookup */
  if (!base_mnt && base_dentry) {
    /* Try to find a mount for this dentry */
    struct path temp_path;
    temp_path.dentry = base_dentry;
    temp_path.mnt = NULL;

    base_mnt = lookup_mnt(&temp_path);

    if (!base_mnt && base_dentry->d_sb) {
      /* Fall back to first mount in superblock's mount list */
      spin_lock(&base_dentry->d_sb->s_mounts_lock);
      if (!list_empty(&base_dentry->d_sb->s_mounts)) {
        struct vfsmount* mnt = list_first_entry(&base_dentry->d_sb->s_mounts,
                                                struct vfsmount, mnt_instance);
        base_mnt = mnt;
      }
      spin_unlock(&base_dentry->d_sb->s_mounts_lock);
    }
  }

  /* Initialize path with base dentry/mount */
  dentry = get_dentry(base_dentry); /* Increment dentry reference */

  /* Handle absolute paths - start from root */
  if (path_str[0] == '/') {
    /* Use filesystem root */
    if (base_mnt) {
      put_dentry(dentry);
      dentry = get_dentry(base_mnt->mnt_root);
    }
    /* Skip leading slash */
    path_str++;
  }

  /* Empty path means current directory */
  if (!*path_str) {
    result->dentry = dentry;
    result->mnt = base_mnt;
    return 0;
  }

  /* Make a copy of the path so we can modify it */
  path_copy = kmalloc(strlen(path_str) + 1);
  if (!path_copy) {
    put_dentry(dentry);
    return -ENOMEM;
  }

  strcpy(path_copy, path_str);
  component = path_copy;

  /* Walk the path component by component */
  while (*component) {
    /* Find the next slash or end of string */
    next_slash = strchr(component, '/');
    if (next_slash) {
      *next_slash = '\0'; /* Temporarily terminate component */
      next_slash++;       /* Move to next component */
    } else {
      next_slash =
          component + strlen(component); /* Point to the null terminator */
    }

    len = strlen(component);

    /* Handle "." - current directory */
    if (len == 1 && component[0] == '.') {
      /* Do nothing - dentry stays the same */
      component = next_slash;
      continue;
    }

    /* Handle ".." - parent directory */
    if (len == 2 && component[0] == '.' && component[1] == '.') {
      struct dentry* parent;

      /* Check if we're at the root already */
      if (dentry == dentry->d_sb->s_root &&
          (!base_mnt || base_mnt->mnt_parent == base_mnt)) {
        /* Already at root - stay at root */
        component = next_slash;
        continue;
      }

      /* Check if we're at a mountpoint */
      if (base_mnt && dentry == base_mnt->mnt_root) {
        /* Cross mount point to parent */
        if (base_mnt->mnt_parent != base_mnt) {
          struct dentry* mnt_parent = base_mnt->mnt_parent->mnt_mountpoint;
          struct vfsmount* parent_mnt = base_mnt->mnt_parent;

          put_dentry(dentry);
          base_mnt = parent_mnt;
          dentry = get_dentry(mnt_parent);
          component = next_slash;
          continue;
        }
      }

      /* Regular parent directory */
      parent = dentry->d_parent;
      if (parent) {
        put_dentry(dentry);
        dentry = get_dentry(parent);
      }

      component = next_slash;
      continue;
    }

    /* Skip empty components (like consecutive slashes) */
    if (len == 0) {
      component = next_slash;
      continue;
    }

    /* Lookup the next component in the current directory */
    inode = dentry->d_inode;
    if (!inode) {
      /* Negative dentry doesn't have an inode */
      put_dentry(dentry);
      kfree(path_copy);
      return -ENOENT;
    }

    /* Check if current dentry is a directory */
    if (!S_ISDIR(inode->i_mode)) {
      put_dentry(dentry);
      kfree(path_copy);
      return -ENOTDIR;
    }

    /* Create qstr for the component */
    struct qstr qname;
    qname.name = component;
    qname.len = len;
    qname.hash = full_name_hash(component, len);

    /* Look up the component in the current directory */
    struct dentry* next = d_lookup(dentry, &qname);
    if (!next) {
      /* Not found in dcache, ask the filesystem */
      if (!inode->i_op || !inode->i_op->lookup) {
        put_dentry(dentry);
        kfree(path_copy);
        return -ENOTDIR;
      }

      /* Allocate a new dentry for this component */
      next = d_alloc(dentry, &qname);
      if (!next) {
        put_dentry(dentry);
        kfree(path_copy);
        return -ENOMEM;
      }

      /* Call the filesystem's lookup method */
      struct dentry* found = inode->i_op->lookup(inode, next, 0);
      if (IS_ERR(found)) {
        d_drop(next);
        put_dentry(next);
        put_dentry(dentry);
        kfree(path_copy);
        return PTR_ERR(found);
      }

      /* If lookup returned a different dentry, use that one */
      if (found) {
        put_dentry(next);
        next = found;
      }
    }

    /* Release the parent dentry */
    put_dentry(dentry);
    dentry = next;

    /* Handle mount points if needed */
    if (base_mnt && dentry->d_flags & DCACHE_MOUNTED) {
      struct vfsmount* mnt = NULL;
      /* Find the mount for this mountpoint - this would typically
         involve looking up in the mount hash table */

      if (mnt) {
        base_mnt = mnt;
        put_dentry(dentry);
        dentry = get_dentry(mnt->mnt_root);
      }
    }

    /* Handle symbolic links if needed and LOOKUP_FOLLOW is set */
    if (dentry->d_inode && S_ISLNK(dentry->d_inode->i_mode) &&
        (flags & LOOKUP_FOLLOW)) {
      /* Implement symlink resolution here */
      /* For now return error as symlinks aren't fully implemented */
      put_dentry(dentry);
      kfree(path_copy);
      return -ENOSYS; /* Not implemented */
    }

    /* Move to the next component */
    component = next_slash;
  }

  /* Set the result */
  result->dentry = dentry;
  result->mnt = base_mnt;

  kfree(path_copy);
  return 0;
}

// Add these functions to your existing file
