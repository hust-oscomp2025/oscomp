#ifndef _FILE_H
#define _FILE_H

#include <kernel/fs/vfs.h>
#include <kernel/types.h>
#include <util/atomic.h>
#include <util/spinlock.h>

/* Forward declarations */
struct dentry;
struct inode;
struct address_space;
struct task_struct;
struct kiocb;
struct iov_iter;
struct pipe_inode_info;
struct poll_table_struct;
struct vm_area_struct;
struct dir_context;

/**
 * Represents an open file in the system
 */
struct file {
	spinlock_t f_lock;
	// 一般来说，file只需要一个成员锁，使用inode的锁保护写入操作
	atomic_t f_refcount;

	/* File identity */
	struct path f_path;    /* Path to file */
	struct inode* f_inode; /* Inode of the file */

	/* File state */
	fmode_t f_mode;       /* File access mode */
	loff_t f_pos;         /* Current file position */
	unsigned int f_flags; /* Kernel internal flags */

	/* Memory management */
	//struct address_space* f_mapping; /* Page cache mapping */
	// 直接在inode里边

	/* Private data */
	void* f_private; /* Filesystem/driver private data */

	/* Data read-ahead */
	struct file_ra_state {
		unsigned long start;      /* Current window start */
		unsigned long size;       /* Size of read-ahead window */
		unsigned long async_size; /* Async read-ahead size */
		unsigned int ra_pages;    /* Maximum pages to read ahead */
		unsigned int mmap_miss;   /* Cache miss stat for mmap */
		unsigned int prev_pos;
	} f_read_ahead;               /* Read-ahead state */

	const struct file_operations* f_operations; /* File sb_operations */
};

/*
 * File API functions
 */
/*打开或创建文件*/
struct file* file_open(const char* filename, int flags, fmode_t mode);
struct file* file_openPath(const struct path* path, int flags, fmode_t mode);

struct file* file_get(struct file* file);
void file_put(struct file* file);

/*位置与访问管理*/
int file_setPos(struct file* file, loff_t pos);
inline loff_t file_getPos(struct file* file);
int file_denyWrite(struct file* file);
int file_allowWrite(struct file* file);
inline bool file_readable(struct file* file);
inline bool file_writable(struct file* file);

/*状态管理与通知*/
int file_setAccessed(struct file *file);
int file_setModified(struct file *file);


/**
 * File operation structure - provides methods for file manipulation
 */
struct file_operations {
	/* Position manipulation */
	loff_t (*llseek)(struct file*, loff_t, int);

	/* Basic I/O */
	ssize_t (*read)(struct file*, char*, size_t, loff_t*);
	ssize_t (*write)(struct file*, const char*, size_t, loff_t*);

	/* Vectored I/O */
	ssize_t (*read_iter)(struct kiocb*, struct iov_iter*);
	ssize_t (*write_iter)(struct kiocb*, struct iov_iter*);

	/* Directory sb_operations */
	int (*iterate)(struct file*, struct dir_context*);
	int (*iterate_shared)(struct file*, struct dir_context*);

	/* Polling/selection */
	__poll_t (*poll)(struct file*, struct poll_table_struct*);

	/* Management sb_operations */
	int (*open)(struct inode*, struct file*);
	int (*flush)(struct file*);
	int (*release)(struct inode*, struct file*);
	int (*fsync)(struct file*, loff_t, loff_t, int datasync);

	/* Memory mapping */
	int (*mmap)(struct file*, struct vm_area_struct*);

	/* Special sb_operations */
	long (*unlocked_ioctl)(struct file*, unsigned int, unsigned long);
	int (*fasync)(int, struct file*, int);

	/* Splice sb_operations */
	ssize_t (*splice_read)(struct file*, loff_t*, struct pipe_inode_info*, size_t, unsigned int);
	ssize_t (*splice_write)(struct pipe_inode_info*, struct file*, loff_t*, size_t, unsigned int);

	/* Space allocation */
	long (*fallocate)(struct file*, int, loff_t, loff_t);
};


/**
 * Open flags - used when opening files
 */
#define O_ACCMODE 00000003         /* Access mode mask */
#define O_RDONLY 00000000          /* Open read-only */
#define O_WRONLY 00000001          /* Open write-only */
#define O_RDWR 00000002            /* Open read-write */
#define O_CREAT 00000100           /* Create if nonexistent */
#define O_EXCL 00000200            /* Error if already exists */
#define O_NOCTTY 00000400          /* Don't assign controlling terminal */
#define O_TRUNC 00001000           /* Truncate to zero length */
#define O_APPEND 00002000          /* Append to file */
#define O_NONBLOCK 00004000        /* Non-blocking I/O */
#define O_DSYNC 00010000           /* Synchronize data */
#define O_SYNC (O_DSYNC | O_RSYNC) /* Synchronize data and metadata */
#define O_RSYNC 00040000           /* Synchronize read sb_operations */
#define O_DIRECT 00100000          /* Direct I/O */
#define O_DIRECTORY 00200000       /* Must be a directory */
#define O_NOFOLLOW 00400000        /* Don't follow symbolic links */
#define O_CLOEXEC 02000000         /* Close on exec */
#define O_PATH 010000000           /* Path-only access */


/* 文件预读(readahead)相关常量 */
#define READ_AHEAD_DEFAULT      16      /* 默认预读窗口大小(页) */
#define READ_AHEAD_MAX          128     /* 最大预读页数 */
#define READ_AHEAD_MIN          4       /* 最小预读窗口大小 */
#define READ_AHEAD_ASYNC_RATIO  2       /* 异步预读与同步预读的比例 */

/* 特殊文件类型预读参数 */
#define READ_AHEAD_PIPE         16      /* 管道预读大小 */
#define READ_AHEAD_SOCKET       8       /* 套接字预读大小 */
#define READ_AHEAD_TTY          4       /* 终端预读大小 */

/*
 * File mode flags
 */
/* Access modes */
#define FMODE_READ (1U << 0)  /* File is open for reading */
#define FMODE_WRITE (1U << 1) /* File is open for writing */
#define FMODE_EXEC (1U << 5)  /* File is executable */

/* Seeking flags */
#define FMODE_LSEEK (1U << 2)  /* File is seekable */
#define FMODE_PREAD (1U << 3)  /* File supports pread */
#define FMODE_PWRITE (1U << 4) /* File supports pwrite */

/* Special access flags */
#define FMODE_ATOMIC_POS (1U << 12) /* File needs atomic access to position */
#define FMODE_RANDOM (1U << 13)     /* File will be accessed randomly */
#define FMODE_PATH (1U << 14)       /* O_PATH flag - minimal file access */
#define FMODE_STREAM (1U << 16)     /* File is stream-like */

/* Permission indicators */
#define FMODE_WRITER (1U << 17)    /* Has write access to underlying fs */
#define FMODE_CAN_READ (1U << 18)  /* Has read methods */
#define FMODE_CAN_WRITE (1U << 19) /* Has write methods */

/* State flags */
#define FMODE_OPENED (1U << 20)  /* File has been opened */
#define FMODE_CREATED (1U << 21) /* File was created */

/* Optimization flags */
#define FMODE_NOWAIT (1U << 22)      /* Return -EAGAIN if I/O would block */
#define FMODE_CAN_ODIRECT (1U << 24) /* Supports direct I/O */
#define FMODE_BUF_RASYNC (1U << 28)  /* Supports async buffered reads */
#define FMODE_BUF_WASYNC (1U << 29)  /* Supports async buffered writes */

/* File reading and writing */
ssize_t kernel_read(struct file* file, void* buf, size_t count, loff_t* pos);
ssize_t kernel_write(struct file* file, const void* buf, size_t count, loff_t* pos);

#endif /* _FILE_H */