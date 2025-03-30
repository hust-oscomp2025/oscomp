Yes, you can definitely simplify the console file creation process given your boot sequence. Since VFS is initialized before the init process starts, you can use the standard VFS methods instead of manual allocation.

Here's a simplified approach:

### 1. During VFS initialization:

```c
int32 console_init(void) {
    dev_t dev = MKDEV(MEM_MAJOR, 1); // Console device
    
    // Register the console character device driver
    register_chrdev(MEM_MAJOR, "mem", &console_fops);
    
    // Create the /dev directory if it doesn't exist
    struct path root_path;
    path_create("/", 0, &root_path);
    vfs_mkdir(root_path.dentry, "dev", 0755);
    path_destroy(&root_path);
    
    // Create the device node
    struct path dev_path;
    path_create("/dev", 0, &dev_path);
    vfs_mknod(dev_path.dentry, "console", S_IFCHR | 0600, dev);
    path_destroy(&dev_path);
    
    return 0;
}
```

### 2. Then your `setup_init_fds` becomes much simpler:

```c
int32 setup_init_fds(struct task_struct *init_task) {
    int32 fd, console_fd;
    struct task_struct *saved_task = current_task();
    
    // Temporarily set current to init task
    set_current_task(init_task);
    
    // Open console device using standard VFS path
    console_fd = vfs_open("/dev/console", O_RDWR);
    if (console_fd < 0) {
        sprint("Failed to open /dev/console: %d\n", console_fd);
        set_current_task(saved_task);
        return console_fd;
    }
    
    // Set up stdin, stdout, stderr
    for (fd = 0; fd < 3; fd++) {
        if (do_dup2(init_task->files, console_fd, fd) < 0) {
            set_current_task(saved_task);
            return -EMFILE;
        }
    }
    
    // Close the original fd if it's not one of stdin/stdout/stderr
    if (console_fd >= 3)
        do_close(console_fd);
    
    // Restore original task
    set_current_task(saved_task);
    return 0;
}
```

### 3. Remove the `create_kernel_console` function entirely

This approach:

1. Uses standard VFS operations (vfs_mkdir, vfs_mknod)
2. Creates a proper device node in /dev
3. Opens the console using the standard file path
4. Simplifies error handling

The key insight is that since VFS is already initialized when you're setting up the init process, you can leverage the full VFS infrastructure rather than manually creating special files.