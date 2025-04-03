#include <kernel/sched.h>
#include <kernel/vfs.h>
#include <kernel/syscall/syscall.h>
#include <kernel/mmu.h>
#include <kernel/util.h>

/* Syscall implementation for openat */
int64 sys_openat(int32 dirfd, const char* pathname, int32 flags, mode_t mode) {
	if (!pathname) return -EFAULT;

	/* Copy pathname from user space */
	char* kpathname = kmalloc(PATH_MAX);
	if (!kpathname) return -ENOMEM;

	if (copy_from_user(kpathname, pathname, PATH_MAX)) {
		kfree(kpathname);
		return -EFAULT;
	}

	/* Call internal implementation */
	int32 ret = do_openat(dirfd, kpathname, flags, mode);
	kfree(kpathname);
	return ret;
}

/**
 * Kernel-internal implementation of openat syscall
 *
 * @param dirfd: File descriptor for directory to base relative paths on, or AT_FDCWD
 * @param pathname: Path to open
 * @param flags: File open flags
 * @param mode: File creation mode if O_CREAT is specified in flags
 * @return: New file descriptor on success, negative error code on failure
 */
int32 do_openat(int32 dirfd, const char* pathname, int32 flags, mode_t mode) {
	struct file* filp = NULL;

	int32 ret;
	bool start_path_valid = false;

	/* Validate open flags */
	ret = validate_open_flags(flags);
	if (ret < 0) {
		return ret;
	}

	/* Perform lookup relative to the starting directory */
	struct path path = {0};
	/* Lookup the file */
	ret = filename_lookup(dirfd, pathname, open2lookup(flags), &path, NULL);
	if (ret < 0) {
		return ret;
	}

	/* Open the file using the resolved path */
	filp = vfs_alloc_file(&path, flags, mode);
	ret = file_open(filp, flags);
	if (ret < 0) {
		path_destroy(&path);
		return PTR_ERR(filp);
	}

	/* Allocate a file descriptor */
	int32 fd = fdtable_allocFd(current_task()->fdtable, 0);
	if (fd < 0) {
		file_unref(filp);
		path_destroy(&path);
		return fd;
	}

	/* Install the file in the fd table */
	fdtable_installFd(current_task()->fdtable, fd, filp);
	return fd;
}

/* Simple wrapper for do_openat with AT_FDCWD */
int32 do_open(const char* pathname, int32 flags, mode_t mode) { return do_openat(AT_FDCWD, pathname, flags, mode); }