# Implementing a RamFS for InitramFS Parsing

Creating a ramfs filesystem for your kernel's initramfs involves several components that work together during early boot. Here's a comprehensive approach:

## 1. Basic RamFS Structure

First, let's create the core ramfs files:

```c
#ifndef _RAMFS_H
#define _RAMFS_H

#include <kernel/vfs.h>

/* RamFS-specific inode data */
struct ramfs_inode_info {
    struct list_head i_list;      /* List of directory entries if dir */
    char *i_data;                 /* Data if regular file */
    size_t i_size;                /* Size of data */
    unsigned int i_blocks;        /* Number of blocks allocated */
    struct timespec i_atime;      /* Access time */
    struct timespec i_mtime;      /* Modification time */
    struct timespec i_ctime;      /* Change time */
};

/* RamFS super block operations */
extern const struct superblock_operations ramfs_sb_ops;

/* RamFS inode operations */
extern const struct inode_operations ramfs_dir_inode_ops;
extern const struct inode_operations ramfs_file_inode_ops;
extern const struct inode_operations ramfs_symlink_inode_ops;

/* RamFS file operations */
extern const struct file_operations ramfs_dir_operations;
extern const struct file_operations ramfs_file_operations;

/* Registration functions */
int ramfs_init(void);
struct fstype *ramfs_get_fstype(void);

#endif /* _RAMFS_H */
```

## 2. RamFS Implementation

```c
#include <kernel/fs/ramfs/ramfs.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/util/string.h>
#include <kernel/time.h>

/* Super block operations */
static struct inode *ramfs_alloc_inode(struct superblock *sb);
static void ramfs_destroy_inode(struct inode *inode);
static int ramfs_write_inode(struct inode *inode, int wait);
static void ramfs_put_super(struct superblock *sb);
static int ramfs_statfs(struct superblock *sb, struct statfs *stat);

const struct superblock_operations ramfs_sb_ops = {
    .alloc_inode = ramfs_alloc_inode,
    .destroy_inode = ramfs_destroy_inode,
    .write_inode = ramfs_write_inode,
    .put_super = ramfs_put_super,
    .statfs = ramfs_statfs,
};

/* File system type */
static int ramfs_fill_super(struct superblock *sb, void *data, int silent);

static struct fstype ramfs_fs_type = {
    .fs_name = "ramfs",
    .fs_mount = ramfs_fill_super,
    .fs_flags = FS_REQUIRES_DEV | FS_USABLE_ROOT,
};

/**
 * ramfs_alloc_inode - Allocate and initialize a new ramfs inode
 * @sb: Superblock for the filesystem
 *
 * Returns: Newly allocated inode or NULL on failure
 */
static struct inode *ramfs_alloc_inode(struct superblock *sb)
{
    struct inode *inode;
    struct ramfs_inode_info *ri;
    
    /* Allocate memory for the inode */
    inode = inode_alloc(sb);
    if (!inode)
        return NULL;
    
    /* Allocate memory for ramfs-specific data */
    ri = kmalloc(sizeof(struct ramfs_inode_info));
    if (!ri) {
        inode_free(inode);
        return NULL;
    }
    
    /* Initialize ramfs-specific data */
    memset(ri, 0, sizeof(struct ramfs_inode_info));
    INIT_LIST_HEAD(&ri->i_list);
    
    /* Store the ramfs-specific data in the inode */
    inode->i_private = ri;
    
    return inode;
}

/**
 * ramfs_destroy_inode - Free ramfs-specific inode data
 * @inode: Inode to clean up
 */
static void ramfs_destroy_inode(struct inode *inode)
{
    struct ramfs_inode_info *ri = (struct ramfs_inode_info *)inode->i_private;
    
    if (ri) {
        /* Free data buffer if this is a regular file */
        if (S_ISREG(inode->i_mode) && ri->i_data)
            kfree(ri->i_data);
            
        kfree(ri);
        inode->i_private = NULL;
    }
    
    inode_free(inode);
}

/**
 * ramfs_write_inode - Write changes to an inode
 * @inode: Inode to update
 * @wait: Whether to wait for I/O to complete
 */
static int ramfs_write_inode(struct inode *inode, int wait)
{
    /* Nothing to do for in-memory filesystem */
    return 0;
}

/**
 * ramfs_put_super - Clean up when unmounting
 * @sb: Superblock to clean up
 */
static void ramfs_put_super(struct superblock *sb)
{
    /* Nothing special to clean up */
}

/**
 * ramfs_statfs - Get filesystem statistics
 * @sb: Superblock for the filesystem
 * @stat: Structure to fill with statistics
 */
static int ramfs_statfs(struct superblock *sb, struct statfs *stat)
{
    /* Fill with basic stats (all memory available) */
    memset(stat, 0, sizeof(struct statfs));
    stat->f_type = RAMFS_MAGIC;
    stat->f_bsize = PAGE_SIZE;
    stat->f_namelen = NAME_MAX;
    
    return 0;
}

/**
 * ramfs_init - Initialize and register the ramfs filesystem
 */
int ramfs_init(void)
{
    return fstype_register(&ramfs_fs_type);
}

struct fstype *ramfs_get_fstype(void)
{
    return &ramfs_fs_type;
}

/**
 * ramfs_fill_super - Fill a superblock for ramfs
 * @sb: Superblock to fill
 * @data: Mount data (unused)
 * @silent: Whether to suppress error messages
 */
static int ramfs_fill_super(struct superblock *sb, void *data, int silent)
{
    struct inode *root;
    
    /* Set up the superblock */
    sb->s_blocksize = PAGE_SIZE;
    sb->s_blocksize_bits = PAGE_SHIFT;
    sb->s_magic = RAMFS_MAGIC;
    sb->s_operations = &ramfs_sb_ops;
    sb->s_maxbytes = MAX_LFS_FILESIZE;
    sb->s_time_gran = 1; /* 1 nanosecond */
    
    /* Create the root inode */
    root = ramfs_alloc_inode(sb);
    if (!root)
        return -ENOMEM;
    
    root->i_mode = S_IFDIR | 0755;
    root->i_uid = 0;
    root->i_gid = 0;
    root->i_size = 0;
    root->i_atime = root->i_mtime = root->i_ctime = CURRENT_TIME;
    
    root->i_op = &ramfs_dir_inode_ops;
    root->i_fop = &ramfs_dir_operations;
    
    /* Set up the root directory */
    sb->s_global_root_dentry = d_make_root(root);
    if (!sb->s_global_root_dentry) {
        inode_put(root);
        return -ENOMEM;
    }
    
    return 0;
}
```

## 3. RamFS File Operations

```c
#include <kernel/fs/ramfs/ramfs.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/util/string.h>

static ssize_t ramfs_read(struct file *file, char *buf, size_t count, loff_t *pos);
static ssize_t ramfs_write(struct file *file, const char *buf, size_t count, loff_t *pos);

const struct file_operations ramfs_file_operations = {
    .read = ramfs_read,
    .write = ramfs_write,
    .llseek = generic_file_llseek,
};

/**
 * ramfs_read - Read data from a ramfs file
 * @file: File to read from
 * @buf: Buffer to read into
 * @count: Number of bytes to read
 * @pos: File position to read from
 */
static ssize_t ramfs_read(struct file *file, char *buf, size_t count, loff_t *pos)
{
    struct inode *inode = file->f_path.dentry->d_inode;
    struct ramfs_inode_info *ri = inode->i_private;
    ssize_t available;
    
    if (!buf || !pos)
        return -EINVAL;
        
    /* Check if we're at EOF */
    if (*pos >= inode->i_size)
        return 0;
        
    /* Calculate how many bytes we can read */
    available = inode->i_size - *pos;
    if (count > available)
        count = available;
        
    /* Copy data to user buffer */
    if (copy_to_user(buf, ri->i_data + *pos, count))
        return -EFAULT;
        
    /* Update position */
    *pos += count;
    
    return count;
}

/**
 * ramfs_write - Write data to a ramfs file
 * @file: File to write to
 * @buf: Buffer to write from
 * @count: Number of bytes to write
 * @pos: File position to write at
 */
static ssize_t ramfs_write(struct file *file, const char *buf, size_t count, loff_t *pos)
{
    struct inode *inode = file->f_path.dentry->d_inode;
    struct ramfs_inode_info *ri = inode->i_private;
    size_t new_size;
    char *new_data;
    
    if (!buf || !pos)
        return -EINVAL;
        
    /* Calculate new file size */
    new_size = *pos + count;
    if (new_size > inode->i_size) {
        /* Need to expand the buffer */
        new_data = krealloc(ri->i_data, new_size + 1);
        if (!new_data)
            return -ENOMEM;
            
        /* Zero out the new space */
        memset(new_data + inode->i_size, 0, new_size - inode->i_size + 1);
        
        ri->i_data = new_data;
        inode->i_size = new_size;
    }
    
    /* Copy data from user buffer */
    if (copy_from_user(ri->i_data + *pos, buf, count))
        return -EFAULT;
        
    /* Update position */
    *pos += count;
    
    /* Update timestamps */
    inode->i_mtime = inode->i_ctime = CURRENT_TIME;
    
    return count;
}
```

## 4. RamFS Directory Operations

```c
#include <kernel/fs/ramfs/ramfs.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/util/string.h>

/* Directory entries */
struct ramfs_dir_entry {
    struct list_head list;
    struct dentry *dentry;
    char *name;
    unsigned int len;
};

static int ramfs_readdir(struct file *file, struct dir_context *ctx);
static int ramfs_create(struct inode *dir, struct dentry *dentry, mode_t mode);
static struct dentry *ramfs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);
static int ramfs_mkdir(struct inode *dir, struct dentry *dentry, mode_t mode);
static int ramfs_rmdir(struct inode *dir, struct dentry *dentry);
static int ramfs_unlink(struct inode *dir, struct dentry *dentry);

const struct file_operations ramfs_dir_operations = {
    .readdir = ramfs_readdir,
};

const struct inode_operations ramfs_dir_inode_ops = {
    .lookup = ramfs_lookup,
    .create = ramfs_create,
    .mkdir = ramfs_mkdir,
    .rmdir = ramfs_rmdir,
    .unlink = ramfs_unlink,
};

/**
 * ramfs_readdir - Read directory entries
 * @file: Directory file
 * @ctx: Directory context for iteration
 */
static int ramfs_readdir(struct file *file, struct dir_context *ctx)
{
    struct inode *inode = file->f_path.dentry->d_inode;
    struct ramfs_inode_info *ri = inode->i_private;
    struct ramfs_dir_entry *de;
    int i = 0;
    
    /* Skip entries that have already been read */
    if (ctx->pos > 0) {
        list_for_each_entry(de, &ri->i_list, list) {
            if (i++ == ctx->pos - 2)
                break;
        }
    } else {
        /* Add "." and ".." entries */
        if (!dir_emit_dot(file, ctx))
            return 0;
        if (!dir_emit_dotdot(file, ctx))
            return 0;
    }
    
    /* Emit regular entries */
    list_for_each_entry_from(de, &ri->i_list, list) {
        if (!dir_emit(ctx, de->name, de->len,
                     de->dentry->d_inode->i_ino, 
                     de->dentry->d_inode->i_mode >> 12))
            return 0;
        ctx->pos++;
    }
    
    return 0;
}

/* ...remaining directory operations... */
```

## 5. InitramFS Parsing

Now, let's implement the initramfs parsing:

```c
#include <kernel/fs/ramfs/ramfs.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/util/string.h>
#include <spike_interface/spike_utils.h>

/* CPIO format constants */
#define CPIO_MAGIC "070701"
#define CPIO_MAGIC_LEN 6
#define CPIO_HEADER_SIZE 110
#define CPIO_FOOTER_NAME "TRAILER!!!"

struct cpio_header {
    char magic[6];        /* "070701" for new ASCII format */
    char ino[8];          /* Inode number */
    char mode[8];         /* Permissions and file type */
    char uid[8];          /* User ID */
    char gid[8];          /* Group ID */
    char nlink[8];        /* Number of links */
    char mtime[8];        /* Modification time */
    char filesize[8];     /* Size of data */
    char major[8];        /* Device major number */
    char minor[8];        /* Device minor number */
    char rmajor[8];       /* Device major for special files */
    char rminor[8];       /* Device minor for special files */
    char namesize[8];     /* Length of filename including NUL */
    char checksum[8];     /* Checksum (unused) */
};

/* Forward declarations */
static int parse_cpio_header(struct cpio_header *hdr, unsigned long *mode, 
                            unsigned long *filesize, unsigned long *namesize);
static struct dentry *create_file(struct dentry *root, const char *pathname, 
                                 mode_t mode, const void *data, size_t size);
static struct dentry *create_directory(struct dentry *root, const char *pathname, mode_t mode);

/**
 * extract_initramfs - Parse and extract initramfs image
 * @initramfs_data: Pointer to the initramfs archive
 * @size: Size of the archive
 * @mount: Mount point for the ramfs
 */
int extract_initramfs(const char *initramfs_data, size_t size, struct vfsmount *mount)
{
    struct dentry *root;
    const char *ptr = initramfs_data;
    const char *end = initramfs_data + size;
    struct cpio_header *hdr;
    unsigned long mode, filesize, namesize;
    char filename[PATH_MAX];
    
    /* Get the root directory */
    if (!mount || !mount->mnt_root)
        return -EINVAL;
    
    root = mount->mnt_root;
    
    /* Process the CPIO archive */
    while (ptr + CPIO_HEADER_SIZE < end) {
        /* Check for alignment */
        ptr = (const char *)(((unsigned long)ptr + 3) & ~3);
        if (ptr + CPIO_HEADER_SIZE >= end)
            break;
            
        /* Parse the header */
        hdr = (struct cpio_header *)ptr;
        ptr += CPIO_HEADER_SIZE;
        
        /* Check magic */
        if (memcmp(hdr->magic, CPIO_MAGIC, CPIO_MAGIC_LEN) != 0) {
            sprint("InitramFS: Invalid CPIO header magic\n");
            return -EINVAL;
        }
        
        /* Parse header fields */
        if (parse_cpio_header(hdr, &mode, &filesize, &namesize) != 0) {
            sprint("InitramFS: Failed to parse CPIO header\n");
            return -EINVAL;
        }
        
        /* Get filename */
        if (namesize >= PATH_MAX || ptr + namesize > end) {
            sprint("InitramFS: Filename too long or beyond archive end\n");
            return -EINVAL;
        }
        
        memcpy(filename, ptr, namesize);
        filename[namesize] = '\0';
        ptr += namesize;
        
        /* Align to 4-byte boundary */
        ptr = (const char *)(((unsigned long)ptr + 3) & ~3);
        
        /* Check for trailer */
        if (strcmp(filename, CPIO_FOOTER_NAME) == 0)
            break;
            
        /* Process the entry */
        if (S_ISREG(mode)) {
            /* Regular file */
            if (ptr + filesize > end) {
                sprint("InitramFS: File data extends beyond archive end\n");
                return -EINVAL;
            }
            
            create_file(root, filename, mode, ptr, filesize);
            ptr += filesize;
        } else if (S_ISDIR(mode)) {
            /* Directory */
            create_directory(root, filename, mode);
        } else {
            /* Skip other types for now (symlinks, devices, etc.) */
            sprint("InitramFS: Skipping non-regular file: %s\n", filename);
            ptr += filesize;
        }
        
        /* Align to 4-byte boundary */
        ptr = (const char *)(((unsigned long)ptr + 3) & ~3);
    }
    
    sprint("InitramFS: Extraction complete\n");
    return 0;
}

/**
 * parse_cpio_header - Parse the numeric fields from a CPIO header
 * @hdr: Header to parse
 * @mode: Output parameter for file mode
 * @filesize: Output parameter for file size
 * @namesize: Output parameter for filename size
 */
static int parse_cpio_header(struct cpio_header *hdr, unsigned long *mode, 
                            unsigned long *filesize, unsigned long *namesize)
{
    char buf[9];
    
    /* Parse mode */
    memcpy(buf, hdr->mode, 8);
    buf[8] = '\0';
    *mode = simple_strtoul(buf, NULL, 16);
    
    /* Parse file size */
    memcpy(buf, hdr->filesize, 8);
    buf[8] = '\0';
    *filesize = simple_strtoul(buf, NULL, 16);
    
    /* Parse name size */
    memcpy(buf, hdr->namesize, 8);
    buf[8] = '\0';
    *namesize = simple_strtoul(buf, NULL, 16);
    
    return 0;
}

/* Implementation of create_file and create_directory functions... */
```

## 6. Mounting InitramFS at Boot

Finally, let's integrate this with the kernel initialization:

```c
/* In your kernel's main initialization function */

void kernel_init(void)
{
    // ... early initialization ...
    
    /* Initialize VFS */
    vfs_init();
    
    /* Initialize RamFS */
    ramfs_init();
    
    /* Create root mount point */
    struct vfsmount *rootmnt = vfs_kern_mount(ramfs_get_fstype(), 0, "rootfs", NULL);
    if (!rootmnt) {
        panic("Failed to mount root filesystem");
    }
    
    /* Set as root filesystem */
    set_root_fs(rootmnt);
    
    /* Extract initramfs if embedded in kernel */
    extern char _binary_initramfs_cpio_start[];
    extern char _binary_initramfs_cpio_end[];
    if (_binary_initramfs_cpio_start != _binary_initramfs_cpio_end) {
        size_t size = _binary_initramfs_cpio_end - _binary_initramfs_cpio_start;
        extract_initramfs(_binary_initramfs_cpio_start, size, rootmnt);
    }
    
    /* Set up working directory */
    init_chdir("/");
    
    // ... continue with kernel initialization ...
}
```

## 7. Including the InitramFS in the Kernel

You'll need to modify your build system to include the initramfs CPIO archive in the kernel binary:

```make
# Add to your kernel build Makefile
initramfs.o: $(INITRAMFS_ARCHIVE)
	$(LD) -r -b binary -o $@ $<

# Ensure initramfs.o is included in your kernel link
```

This implementation provides a complete solution for parsing and mounting an initramfs during kernel boot. The ramfs filesystem serves as a simple but effective initial filesystem that exists entirely in memory, which is perfect for the boot process and early initialization.