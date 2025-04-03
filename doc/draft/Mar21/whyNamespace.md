You raise a good point that touches on an important distinction between visibility of filesystem objects and the concept of mount namespaces.

In a typical desktop environment like Ubuntu on a laptop, you're right that users generally have a unified view of the filesystem. There usually isn't a need for different processes to have different views of the filesystem hierarchy. Most desktop Linux installations use a single mount namespace that's shared by all processes.

Here's what's actually happening:

## Single User Desktop Environment

On your Ubuntu laptop:
1. All processes typically share the same mount namespace
2. The filesystem hierarchy appears unified to all processes
3. Permissions and DAC (Discretionary Access Control) determine what files you can access, not namespace visibility

## Where Mount Namespaces Matter

Mount namespaces become important in these scenarios:

1. **Container technologies** like Docker, LXC, and Kubernetes use mount namespaces to give containers their own isolated filesystem view
2. **Chroot environments** create limited views of the filesystem 
3. **Sandboxed applications** may use mount namespaces for isolation
4. **System services** might use private mounts for security reasons

## The Purpose of vfs_kern_mount

Even in a desktop environment, `vfs_kern_mount` serves important purposes:

1. **Boot time mounting**: During system initialization, the kernel needs to mount the root filesystem before user namespaces are set up
2. **Temporary mounts**: Some kernel operations need temporary mounts that don't need to be visible
3. **Prepared mounts**: Some operations prepare mounts before deciding where they belong

## A Practical Example

When you plug in a USB drive on Ubuntu, here's a simplified flow of what happens:

1. The kernel detects the device
2. A kernel component uses `vfs_kern_mount` to set up the filesystem
3. The mount is then added to the system's mount namespace via something like `do_mount`
4. Now the USB drive is visible in `/media/username/drivename`

The temporary "kernel-only" phase is invisible to you as a user, but it allows the kernel to validate and prepare the filesystem before exposing it to user processes.

## Conclusion

You're right that in typical desktop use, the filesystem is essentially a public resource with a unified view. Mount namespaces and kernel-only mounts are infrastructure that enables more complex scenarios (containers, virtualization, etc.) but aren't noticeable in everyday desktop usage.

The separation between kernel mounting and namespace visibility is part of the kernel's modular design that enables both simple desktop usage and complex server/container deployments using the same core code.