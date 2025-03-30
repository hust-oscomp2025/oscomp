You're asking important questions about the boot sequence and root filesystem management. Let me address each:

### Console Initialization Timing

Yes, `console_init` should be called after loading the initramfs but before initializing the init process:

```
early DTB → VMM init → VFS init → load initramfs → console_init → init process
```

This timing ensures:
1. The filesystem is available to create `/dev/console`
2. The console device is ready before init needs it for stdio

### Root Filesystem Switch Process

The process of switching from the initial ramdisk to the disk filesystem is called "pivot_root" and typically occurs after init starts. Here's how it works:

1. **Initial boot with initramfs**:
   - System boots with initramfs as root
   - Basic drivers are loaded
   - Init process starts

2. **Init script mounts real root**:
   ```c
   // In init script or early userspace program
   mount("/dev/sda1", "/mnt/newroot", "ext4", 0, NULL);
   ```

3. **Move critical filesystems**:
   ```c
   // Move special filesystems to new root
   mount("/proc", "/mnt/newroot/proc", NULL, MS_MOVE, NULL);
   mount("/sys", "/mnt/newroot/sys", NULL, MS_MOVE, NULL);
   mount("/dev", "/mnt/newroot/dev", NULL, MS_MOVE, NULL);
   ```

4. **Execute pivot_root**:
   ```c
   // Switch root filesystem
   pivot_root("/mnt/newroot", "/mnt/newroot/oldroot");
   chdir("/");
   ```

5. **Clean up and execute real init**:
   ```c
   // Unmount old root if possible
   umount2("/oldroot", MNT_DETACH);
   
   // Execute the real init
   execve("/sbin/init", argv, envp);
   ```

This operation moves your system from running on the temporary initramfs to the real filesystem on disk. In your kernel, you'd need to implement:

1. The `pivot_root()` system call
2. Support for mounting and unmounting filesystems
3. Process execution via `execve()`

For a minimal OS, you could simplify by having init directly mount the root filesystem without the pivot_root operation, especially if you don't need the flexibility of an initramfs for hardware detection.