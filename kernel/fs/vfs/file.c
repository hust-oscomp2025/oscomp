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

static struct file* __alloc_file(struct dentry* dentry, fmode_t mode);
static struct file* __do_file_open(struct dentry* dentry, struct vfsmount* mnt,
                                   int flags, mode_t mode);
static int __file_close(struct file* filp, struct task_struct* owner);

/**
 * file_open_path - Open a file using a pre-resolved path
 * @path: Path to the file
 * @flags: Open flags
 * @mode: Creation mode
 */
struct file* file_open_path(const struct path* path, int flags, mode_t mode) {
	if (!path || !path->dentry)
		return ERR_PTR(-EINVAL);

	return __do_file_open(path->dentry, path->mnt, flags, mode);
}

/**
 * file_open_qstr - Open a file using a qstr path
 * @name: Path as qstr
 * @flags: Open flags
 * @mode: Creation mode
 */
struct file* file_open_qstr(const struct qstr* name, int flags, mode_t mode) {
	struct path path;
	struct file* file;
	int error;

	if (!name)
		return ERR_PTR(-EINVAL);

	/* Convert qstr to path */
	error = kern_path_qstr(name, 0, &path);

	/* Handle O_CREAT flag if file doesn't exist */
	if (error == -ENOENT && (flags & O_CREAT)) {
		/* Would handle file creation here */
		/* For now, just return the error */
		return ERR_PTR(error);
	} else if (error) {
		return ERR_PTR(error);
	}

	/* Open the file */
	file = file_open_path(&path, flags, mode);

	/* Release path reference */
	put_path(&path);

	return file;
}

/**
 * file_open - Traditional file open with char* path
 * @filename: Path to file
 * @flags: Open flags
 * @mode: Creation mode
 */
struct file* file_open(const char* filename, int flags, mode_t mode) {
	struct qstr qname;
	struct file* file;

	if (!filename)
		return ERR_PTR(-EINVAL);

	/* Initialize qstr from string */
	qstr_init_from_str(&qname, filename);

	/* Use qstr version */
	file = file_open_qstr(&qname, flags, mode);

	return file;
}

/**
 * __file_close - Close a file and clean up resources
 * @filp: Pointer to the file to close
 * @owner: Task that owns the file descriptor (can be NULL for kernel-owned
 * files)
 *
 * Closes a file, releases resources, updates task's file descriptor table
 * if owner is specified, and calls the file's release operation.
 *
 * Returns 0 on success or negative error code on failure.
 */
int __file_close(struct file* filp, struct task_struct* owner) {
	int error = 0;
	int fd = -1;

	if (!filp)
		return -EINVAL;

	/* If owner specified, find and clear the fd in owner's fd table */
	if (owner && owner->fd_struct) {
		struct fd_struct* files = owner->fd_struct;

		spin_lock(&files->file_lock);

		/* Search for this file in the fd table */
		for (fd = 0; fd < files->max_fds; fd++) {
			if (files->fd_array[fd] == filp) {
				/* Clear the fd entry */
				files->fd_array[fd] = NULL;
				break;
			}
		}

		spin_unlock(&files->file_lock);
	}

	/* Release the file's regular resources */
	if (filp->f_op && filp->f_op->release) {
		/* Call the file-specific release operation */
		error = filp->f_op->release(filp->f_inode, filp);
	}

	/* Handle file-specific cleanup operations */
	if (filp->f_private) {
		/* Some file types might require special cleanup of f_private */
		/* e.g., for pipes, sockets, etc. */
		/* Implementation depends on file type */
	}

	/* Release associated resources */
	put_path(&filp->f_path);

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
 * alloc_fd - Allocate file descriptor and install file
 * @file: File pointer to install
 * @flags: Optional flags for the descriptor (for future use)
 *
 * Atomically allocates an unused file descriptor and installs
 * the provided file pointer. This replaces the separate
 * get_unused_fd() + fd_install() pattern.
 *
 * Returns the new file descriptor or negative error code.
 */
int alloc_fd(struct file* file, unsigned int flags) {
	struct fd_struct* files = current_task()->fd_struct;
	int fd;

	if (!files || !file)
		return -EINVAL;

	spin_lock(&files->file_lock);

	/* Find unused descriptor */
	for (fd = 3; fd < files->max_fds; fd++) {
		if (!files->fd_array[fd]) {
			/* Install the file directly */
			files->fd_array[fd] = file;

			/* Store descriptor flags (when implemented) */
			files->fd_flags[fd] = flags;

			spin_unlock(&files->file_lock);
			return fd;
		}
	}

	spin_unlock(&files->file_lock);
	return -EMFILE; /* Too many open files */
}

/**
 * fd_install - Associate a file descriptor with a file pointer
 * @fd: File descriptor
 * @file: File pointer
 *
 * Associates the given file pointer with the file descriptor in
 * the current task's file descriptor table.
 */
void fd_install(unsigned int fd, struct file* file) {
	struct fd_struct* files = current_task()->fd_struct;

	if (!files || fd >= files->max_fds)
		return;

	spin_lock(&files->file_lock);

	/* Replace placeholder or NULL with actual file */
	files->fd_array[fd] = file;

	spin_unlock(&files->file_lock);
}

/**
 * __do_file_open - Common file opening logic
 * @dentry: Dentry of file to open
 * @mnt: Mount point
 * @flags: Open flags
 * @mode: Creation mode
 *
 * Internal helper function for all file_open variants.
 */
static struct file* __do_file_open(struct dentry* dentry, struct vfsmount* mnt,
                                   int flags, mode_t mode) {
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
	file = __alloc_file(dentry, file_mode);
	if (IS_ERR(file))
		return file;

	/* Additional initialization */
	file->f_flags = flags;
	file->f_path.mnt = get_mount(mnt);

	/* Set position */
	if (flags & O_APPEND)
		file->f_pos = inode->i_size;

	/* Call open method if available */
	if (file->f_op && file->f_op->open) {
		error = file->f_op->open(inode, file);
		if (error) {
			/* Clean up on error */
			put_path(&file->f_path);
			put_inode(file->f_inode);
			kfree(file);
			return ERR_PTR(error);
		}
	}

	return file;
}

/**
 * kernel_read - Read data from a file
 * @file: File to read from
 * @buf: Buffer to read into
 * @count: Number of bytes to read
 * @pos: Position to read from (or NULL to use and update file's position)
 *
 * Provides a unified interface for reading from various file types
 * in the kernel. Handles position pointer management automatically.
 *
 * Returns number of bytes read on success or negative error code
 */
ssize_t kernel_read(struct file* file, void* buf, size_t count, loff_t* pos) {
	loff_t* ppos;
	ssize_t ret;

	if (!file || !file->f_op)
		return -EINVAL;

	if (!file->f_op->read && !file->f_op->read_iter)
		return -EINVAL;

	/* Determine which position to use */
	ppos = pos ? pos : &file->f_pos;

	/* Check permissions */
	if (!(file->f_mode & FMODE_READ))
		return -EBADF;

	/* Call appropriate read implementation */
	if (file->f_op->read)
		ret = file->f_op->read(file, buf, count, ppos);
	else
		ret = -EINVAL; /* read_iter not implemented yet */

	return ret;
}

/**
 * kernel_write - Write data to a file
 * @file: File to write to
 * @buf: Buffer containing data to write
 * @count: Number of bytes to write
 * @pos: Position to write to (or NULL to use and update file's position)
 *
 * Provides a unified interface for writing to various file types
 * in the kernel. Handles position pointer management automatically.
 *
 * Returns number of bytes written on success or negative error code
 */
ssize_t kernel_write(struct file* file, const void* buf, size_t count,
                     loff_t* pos) {
	loff_t* ppos;
	ssize_t ret;

	if (!file || !file->f_op)
		return -EINVAL;

	if (!file->f_op->write && !file->f_op->write_iter)
		return -EINVAL;

	/* Determine which position to use */
	ppos = pos ? pos : &file->f_pos;

	/* Check permissions */
	if (!(file->f_mode & FMODE_WRITE))
		return -EBADF;

	/* Call appropriate write implementation */
	if (file->f_op->write)
		ret = file->f_op->write(file, buf, count, ppos);
	else
		ret = -EINVAL; /* write_iter not implemented yet */

	return ret;
}

/**
 * __alloc_file - Allocates and initializes a new file structure
 * @dentry: The dentry to associate with the file
 * @mode: File mode flags
 *
 * This function allocates and initializes a new file structure with the
 * given dentry and mode. The caller is responsible for setting any additional
 * fields as needed.
 *
 * Returns a pointer to the allocated file structure, or an ERR_PTR on error.
 */
static struct file* __alloc_file(struct dentry* dentry, fmode_t mode) {
	struct file* file;

	if (!dentry)
		return ERR_PTR(-EINVAL);

	/* Allocate file structure */
	file = kmalloc(sizeof(struct file));
	if (!file)
		return ERR_PTR(-ENOMEM);

	/* Initialize to zeros */
	memset(file, 0, sizeof(struct file));

	/* Initialize with basic information */
	file->f_mode = mode;
	atomic_set(&file->f_count, 1);
	spinlock_init(&file->f_lock);

	/* Set path */
	file->f_path.dentry = get_dentry(dentry);
	file->f_path.mnt = NULL; /* Caller must set this if needed */

	/* Set inode if available */
	if (dentry->d_inode) {
		file->f_inode = grab_inode(dentry->d_inode);
		file->f_op = dentry->d_inode->i_fop;
	}

	return file;
}

/**
 * get_file - Get file pointer from file descriptor
 * @fd: File descriptor
 * @owner: Task that owns the file descriptor (NULL uses current task)
 *
 * Retrieves the file pointer associated with a file descriptor
 * from the specified task's file descriptor table and increments
 * its reference count.
 *
 * Returns the file pointer or NULL if the descriptor is invalid.
 */
struct file* get_file(unsigned int fd, struct task_struct* owner) {
	struct task_struct* task = owner ? owner : current_task();
	struct fd_struct* files = task ? task->fd_struct : NULL;
	struct file* file = NULL;

	if (!files || fd >= files->max_fds)
		return NULL;

	spin_lock(&files->file_lock);

	file = files->fd_array[fd];

	/* Only increment reference count for valid file pointers */
	if (file && file != (struct file*)1) {
		atomic_inc(&file->f_count);
	} else {
		file = NULL; /* Return NULL for invalid or placeholder entries */
	}

	spin_unlock(&files->file_lock);

	return file;
}

/**
 * put_file - Decrease reference count on a file
 * @file: File pointer
 * @owner: Task that owns the file descriptor (can be NULL)
 *
 * Decrements the reference count on a file and releases
 * it if the count reaches zero, passing owner information
 * for proper cleanup.
 */
void put_file(struct file* file, struct task_struct* owner) {
	if (!file || file == (struct file*)1)
		return;

	if (atomic_dec_and_test(&file->f_count)) {
		/* Reference count reached zero, close the file */
		__file_close(file, owner);
	}
}
