#ifndef _STAT_H
#define _STAT_H

#include <kernel/types.h>
#include <kernel/fs/vfs/path.h>

/* File types */
#define S_IFMT  00170000  /* Mask for file type */
#define S_IFREG  0100000  /* Regular file */
#define S_IFDIR  0040000  /* Directory */
#define S_IFCHR  0020000  /* Character device */
#define S_IFBLK  0060000  /* Block device */
#define S_IFIFO  0010000  /* FIFO */
#define S_IFLNK  0120000  /* Symbolic link */
#define S_IFSOCK 0140000  /* Socket */

/* Permission bits */
#define S_ISUID 0004000   /* Set user ID on execution */
#define S_ISGID 0002000   /* Set group ID on execution */
#define S_ISVTX 0001000   /* Sticky bit */
#define S_IRWXU 0000700   /* User mask */
#define S_IRUSR 0000400   /* User read permission */
#define S_IWUSR 0000200   /* User write permission */
#define S_IXUSR 0000100   /* User execute permission */
#define S_IRWXG 0000070   /* Group mask */
#define S_IRGRP 0000040   /* Group read permission */
#define S_IWGRP 0000020   /* Group write permission */
#define S_IXGRP 0000010   /* Group execute permission */
#define S_IRWXO 0000007   /* Others mask */
#define S_IROTH 0000004   /* Others read permission */
#define S_IWOTH 0000002   /* Others write permission */
#define S_IXOTH 0000001   /* Others execute permission */

/* File type check macros */
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

/* File dirent types (for directory entries) */
#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK     10
#define DT_SOCK    12

/* Attribute flags for iattr */
#define ATTR_MODE     (1 << 0)
#define ATTR_UID      (1 << 1)
#define ATTR_GID      (1 << 2)
#define ATTR_SIZE     (1 << 3)
#define ATTR_ATIME    (1 << 4)
#define ATTR_MTIME    (1 << 5)
#define ATTR_CTIME    (1 << 6)
#define ATTR_ATIME_SET (1 << 7)
#define ATTR_MTIME_SET (1 << 8)
#define ATTR_FORCE    (1 << 9)

/**
 * struct kstat - Kernel file stat structure
 * Holds all the filesystem metadata information about a file
 */
struct kstat {
    uint64_t     st_dev;     /* Device ID containing file */
    uint64_t     st_ino;     /* File inode number */
    fmode_t      st_mode;    /* File mode and type */
    uint32_t     st_nlink;   /* Number of hard links */
    uid_t        st_uid;     /* User ID of owner */
    gid_t        st_gid;     /* Group ID of owner */
    uint64_t     st_rdev;    /* Device ID (if special file) */
    uint64_t     st_size;    /* File size in bytes */
    uint32_t     st_blksize; /* Block size for filesystem I/O */
    uint64_t     st_blocks;  /* Number of 512B blocks allocated */
    
    /* Time values with nanosecond precision */
    struct timespec64 st_atime; /* Last access time */
    struct timespec64 st_mtime; /* Last modification time */
    struct timespec64 st_ctime; /* Last status change time */
    struct timespec64 st_btime; /* Creation (birth) time */
};

/**
 * struct statfs - User-facing filesystem statistics
 * Populated for the statfs() system call
 */
struct statfs {
    long f_type;   /* Filesystem type */
    long f_bsize;  /* Block size */
    long f_blocks; /* Total blocks */
    long f_bfree;  /* Free blocks */
    long f_bavail; /* Available blocks */
    long f_files;  /* Total inodes */
    long f_ffree;  /* Free inodes */
};

/**
 * struct kstatfs - Internal kernel filesystem statistics
 * Used by the VFS layer for all filesystem operations
 */
struct kstatfs {
    uint64_t f_type;    /* Type of filesystem */
    uint64_t f_bsize;   /* Optimal transfer block size */
    uint64_t f_blocks;  /* Total data blocks in filesystem */
    uint64_t f_bfree;   /* Free blocks in filesystem */
    uint64_t f_bavail;  /* Free blocks available to unprivileged user */
    uint64_t f_files;   /* Total file nodes in filesystem */
    uint64_t f_ffree;   /* Free file nodes in filesystem */
    uint64_t f_namelen; /* Maximum length of filenames */
    uint64_t f_frsize;  /* Fragment size */
    uint64_t f_flags;   /* Mount flags */
};

/**
 * struct iattr - attributes to be changed
 * @ia_valid: bitmask of attributes to change
 * @ia_mode: new file mode
 * @ia_uid: new owner uid
 * @ia_gid: new group id
 * @ia_size: new file size
 * @ia_atime: new access time
 * @ia_mtime: new modification time
 * @ia_ctime: new change time
 */
struct iattr {
    unsigned int ia_valid;
    fmode_t ia_mode;
    uid_t ia_uid;
    gid_t ia_gid;
    loff_t ia_size;
    struct timespec64 ia_atime;
    struct timespec64 ia_mtime;
    struct timespec64 ia_ctime;
};

/* Function prototypes */
int vfs_stat(const char* path, struct kstat* stat);
int vfs_statfs(struct path* path, struct kstatfs* kstatfs);
int vfs_utimes(const char* path, struct timespec* times, int flags);

#endif /* _STAT_H */