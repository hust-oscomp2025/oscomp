#include <kernel/vfs.h>
#include <kernel/sched.h>

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
    struct path start_path = {0};
    int32 ret;

    /* If pathname is absolute or dirfd is AT_FDCWD, behavior is similar to regular open */
    if (pathname[0] == '/' || dirfd == AT_FDCWD) {
        return do_open(pathname, flags, mode);
    }

    /* Get the starting directory from dirfd */
    struct fdtable* fdt = current_task()->fdtable;
    struct file* dir_file = fdtable_getFile(fdt, dirfd);
    if (!dir_file) {
        return -EBADF;
    }

    /* Check if dirfd refers to a directory */
    if (!S_ISDIR(dir_file->f_inode->i_mode)) {
        return -ENOTDIR;
    }

    /* Set the starting directory path */
    start_path = dir_file->f_path;

    /* Perform lookup relative to the starting directory */
    struct path path = {0};
    uint32 lookup_flags = 0;
    
    /* Set lookup flags based on open flags */
    if (!(flags & O_NOFOLLOW)) {
        lookup_flags |= LOOKUP_FOLLOW;
    }
    
    /* Create flag handling */
    bool creating = (flags & O_CREAT) != 0;
    if (creating) {
        lookup_flags |= LOOKUP_CREATE;
    }

    /* Lookup the file */
    ret = filename_lookup(dirfd, pathname, lookup_flags, &path, &start_path);
    if (ret < 0) {
        /* Handle special case when creating a new file */
        if (creating && ret == -ENOENT) {
            /* Try to resolve parent directory path */
            char* last_slash = strrchr(pathname, '/');
            if (last_slash) {
                /* Split into directory and filename */
                char parent_path[PATH_MAX];
                safestrcpy(parent_path, pathname, last_slash - pathname + 1);
                parent_path[last_slash - pathname] = '\0';
                
                /* Get parent directory */
                struct path parent;
                ret = filename_lookup(dirfd, parent_path, lookup_flags, &parent, &start_path);
                if (ret < 0) {
                    return ret;
                }
                
                /* Create file in parent directory */
                /* Note: Actual file creation would be handled at a lower level */
            }
        } else {
            return ret;
        }
    }

    /* Open the file using the resolved path */
    filp = file_open(&path, flags, mode);
    if (PTR_IS_ERROR(filp)) {
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


// 在 syscall 层面调用
int32 do_open(const char *pathname, int32 flags, mode_t mode)
{
    struct path path;
    struct file *file;
    int32 fd, error;
    
    // 解析路径
    error = path_create(pathname, LOOKUP_FOLLOW, &path);
    if (error)
        return error;
    
    // 创建 file 对象和基本初始化
    file = vfs_alloc_file(&path, flags, mode);
    if (IS_ERR(file)) {
        path_destroy(&path);
        return PTR_ERR(file);
    }
    
    // 调用文件系统特定的打开操作
    error = file_open(file, flags, mode);
    if (error) {
        file_unref(file);
        return error;
    }
    
    // 分配文件描述符
    fd = fdtable_installFd(current_task()->fdtable, -1, file);
    if (fd < 0) {
        file_close(file);
        file_unref(file);
        return fd;
    }
    
    return fd;
}
