#include <kernel/fs/dentry.h>
#include <kernel/fs/file.h>
#include <kernel/fs/inode.h>
#include <kernel/fs/namespace.h>
#include <kernel/fs/path.h>
#include <kernel/fs/vfs.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/sched/process.h>
#include <kernel/sched/sched.h>
#include <kernel/types.h>
#include <util/qstr.h>
#include <util/string.h>

#include <spike_interface/spike_utils.h>

static struct file* __file_alloc(struct dentry* dentry, fmode_t mode);
static struct file* __file_open(struct dentry* dentry, struct vfsmount* mnt, int flags, fmode_t mode);
static int __file_free(struct file* filp);

struct file* file_get(struct file* file) {
	if (!file)
		return NULL;
	if (atomic_read(&file->f_refcount) <= 0) {
		panic("file_get: f_refcount is already 0\n");
	}
	/* Increment reference count */
	atomic_inc(&file->f_refcount);
	return file;
}

/**
 * file_openPath - Open a file using a pre-resolved path
 * @path: Path to the file
 * @flags: Open flags
 * @mode: Creation mode
 */
struct file* file_openPath(const struct path* path, int flags, fmode_t mode) {
	if (!path || !path->dentry)
		return ERR_PTR(-EINVAL);

	return __file_open(path->dentry, path->mnt, flags, mode);
}

/**
 * file_open - Traditional file open with char* path
 * @filename: Path to file
 * @flags: Open flags
 * @mode: Creation mode
 */
struct file* file_open(const char* filename, int flags, fmode_t mode) {
	// struct qstr qname;
	struct file* file;

	if (!filename)
		return ERR_PTR(-EINVAL);

	/* Initialize qstr from string */
	// qstr_init_from_str(&qname, filename);

	/* Use qstr version */
	// file = file_open_qstr(&qname, flags, mode);
	/* Convert qstr to path */
	struct path path;
	int error = path_create(&filename, 0, &path);
	/* Handle O_CREAT flag if file doesn't exist */
	if (error == -ENOENT && (flags & O_CREAT)) {
		/* Would handle file creation here */
		/* For now, just return the error */
		return ERR_PTR(error);
	} else if (error) {
		return ERR_PTR(error);
	}
	/* Open the file */
	file = __file_open(path.dentry, path.mnt, flags, mode);
	/* Release path reference */
	path_destroy(&path);

	return file;
}

/**
 * __file_free - Close a file and clean up resources
 * @filp: Pointer to the file to close
 * @owner: Task that owns the file descriptor (can be NULL for kernel-owned
 * files)
 *
 * Closes a file, releases resources, updates task's file descriptor table
 * if owner is specified, and calls the file's release operation.
 *
 * Returns 0 on success or negative error code on failure.
 */
int __file_free(struct file* filp) {
	int error = 0;
	int fd = -1;

	if (!filp)
		return -EINVAL;

	/* Release the file's regular resources */
	if (filp->f_operations && filp->f_operations->release) {
		/* Call the file-specific release operation */
		error = filp->f_operations->release(filp->f_inode, filp);
	}

	/* Handle file-specific cleanup sb_operations */
	if (filp->f_private) {
		/* Some file types might require special cleanup of f_private */
		/* e.g., for pipes, sockets, etc. */
		/* Implementation depends on file type */
	}

	/* Release associated resources */
	path_destroy(&filp->f_path);

	/* Release inode reference */
	if (filp->f_inode)
		put_inode(filp->f_inode);

	/* Log the file close if debugging enabled */
	if (fd >= 0) {
		/* debug_file_close(owner, fd); */
	}

	/* Free the file structure itself */
	kfree(filp);

	return error;
}

/**
 * __file_open - Common file opening logic
 * @dentry: Dentry of file to open
 * @mnt: Mount point
 * @flags: Open flags
 * @mode: Creation mode
 *
 * Internal helper function for all file_open variants.
 */
static struct file* __file_open(struct dentry* dentry, struct vfsmount* mnt, int flags, fmode_t mode) {
	struct file* file;
	struct inode* inode;
	int error = 0;
	fmode_t file_mode;

	/* Validate dentry has an inode */
	inode = dentry->d_inode;
	if (!inode)
		return ERR_PTR(-ENOENT);

	/* Check directory write permissions */
	if (S_ISDIR(inode->i_mode) && (flags & O_ACCMODE) != O_RDONLY)
		return ERR_PTR(-EISDIR);

	/* Convert flags to fmode_t */
	file_mode = FMODE_READ;
	if ((flags & O_ACCMODE) != O_RDONLY)
		file_mode |= FMODE_WRITE;

	/* Allocate and initialize file structure using our new function */
	file = __file_alloc(dentry, file_mode);
	if (IS_ERR(file))
		return file;

	/*initialization */
	atomic_set(&file->f_refcount, 1);
	file->f_path.dentry = get_dentry(dentry);
	file->f_path.mnt = get_mount(mnt);
	/* Set inode if available */
	assert(dentry->d_inode); // 我们的设计确保所有文件系统都有inode和对应的操作
	file->f_inode = grab_inode(dentry->d_inode);
	file->f_operations = dentry->d_inode->i_fop;
	file->f_mode = file_mode;
	file->f_flags = flags;

	file->f_private = NULL;

	/* 设置预读参数 */
	file->f_read_ahead.start = 0;
	file->f_read_ahead.size = READ_AHEAD_DEFAULT;
	file->f_read_ahead.async_size = 0;
	file->f_read_ahead.ra_pages = READ_AHEAD_MAX;
	file->f_read_ahead.mmap_miss = 0;
	file->f_read_ahead.prev_pos = 0;

	/* Set position */
	if (flags & O_APPEND)
		file->f_pos = inode->i_size;

	/* Call open method if available */
	if (file->f_operations && file->f_operations->open) {
		error = file->f_operations->open(inode, file);
		if (error) {
			/* Clean up on error */
			path_destroy(&file->f_path);
			put_inode(file->f_inode);
			kfree(file);
			return ERR_PTR(error);
		}
	}

	return file;
}



/**
 * __file_alloc - Allocatesa new file structure
 *
 * This function allocates a new file structure.
 * The caller is responsible for initializing the file structure.
 *
 * Returns a pointer to the allocated file structure, or an ERR_PTR on error.
 */
static struct file* __file_alloc(struct dentry* dentry, fmode_t mode) {
	struct file* file;

	if (!dentry)
		return ERR_PTR(-EINVAL);

	/* Allocate file structure */
	file = kmalloc(sizeof(struct file));
	if (!file)
		return ERR_PTR(-ENOMEM);

	/* Initialize to zeros */
	memset(file, 0, sizeof(struct file));
	spinlock_init(&file->f_lock);

	return file;
}

/**
 * file_setPos - Set the position of a file
 * @file: File pointer
 * @pos: New position to set
 *
 * Sets the current position of the file to the specified value.
 * Returns 0 on success or negative error code on failure.
 */
int file_setPos(struct file* file, loff_t pos) {
	if (!file)
		return -EINVAL;
	if (!file->f_inode || atomic_read(&file->f_refcount) <= 0)
		return -EBADF;
	if (pos < 0 || pos > file->f_inode->i_size)
		return -EINVAL;
	spinlock_lock(&file->f_lock);
	file->f_pos = pos;
	spinlock_unlock(&file->f_lock);
	return 0;
}

/**
 * file_getPos - Get the current position of a file
 * @file: File pointer
 *
 * Returns the current position of the file or -1 on error.
 */
inline loff_t file_getPos(struct file* file) {
	if (!file || !file->f_inode || atomic_read(&file->f_refcount) <= 0)
		return -1;

	/* Return the current position */
	return file->f_pos;
}

/**
 * file_denyWrite
 * @file: File pointer
 */
int file_denyWrite(struct file* file) {
	if (!file) {
		return -EINVAL;
	}
	if (!file->f_inode || atomic_read(&file->f_refcount) <= 0)
		return -EBADF;

	/* Deny write access by setting the mode to read-only */
	spinlock_lock(&file->f_lock);
	file->f_mode &= ~FMODE_WRITE;
	spinlock_unlock(&file->f_lock);
	return 0;
}

/**
 * file_allowWrite
 * @file: File pointer
 *
 * Allows write access to the file by setting the mode to read-write.
 * Returns 0 on success or negative error code on failure.
 */
int file_allowWrite(struct file* file) {
	if (!file) {
		return -EINVAL;
	}
	if (!file->f_inode || atomic_read(&file->f_refcount) <= 0)
		return -EBADF;

	/* Allow write access by setting the mode to read-write */
	spinlock_lock(&file->f_lock);
	file->f_mode |= FMODE_WRITE;
	spinlock_unlock(&file->f_lock);
	return 0;
}

inline bool file_readable(struct file* file) {
	if (!file || !file->f_inode || atomic_read(&file->f_refcount) <= 0)
		return false;
	return (file->f_mode & FMODE_READ) != 0;
}

inline bool file_writable(struct file* file) {
	if (!file || !file->f_inode || atomic_read(&file->f_refcount) <= 0)
		return false;
	return (file->f_mode & FMODE_WRITE) != 0;
}

/**
 * file_put - Decrease reference count on a file
 * @file: File pointer
 * @owner: Task that owns the file descriptor (can be NULL)
 *
 * Decrements the reference count on a file and releases
 * it if the count reaches zero, passing owner information
 * for proper cleanup.
 */
void file_put(struct file* file) {
	if (!file)
		return;

	if (atomic_dec_and_test(&file->f_refcount)) {
		/* Reference count reached zero, close the file */
		__file_free(file);
	} else {
		panic("file_put: f_refcount is already 0\n");
		/* This should not happen, as we expect the reference count
		 * to be managed properly by the caller */
	}
}
