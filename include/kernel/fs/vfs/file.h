#ifndef _FILE_H
#define _FILE_H

#include <kernel/fs/vfs/vfs.h>
#include <kernel/fs/vfs/path.h>
#include <kernel/types.h>
#include <util/atomic.h>
#include <util/spinlock.h>

typedef uint32 fmode_t;
struct io_vector;
struct io_vector_iterator;
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
	//struct addrSpace* f_mapping; /* Page cache mapping */
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

	const struct file_operations* f_operations; /* File s_operations */
};

#define f_mapping f_inode->i_mapping
#define f_dentry f_path.dentry

/*
 * File API functions
 */
/*打开或创建文件*/
struct file* file_open(const char* filename, int flags, fmode_t mode);
struct file* file_openPath(const struct path* path, int flags, fmode_t mode);

struct file* file_get(struct file* file);
void file_put(struct file* file);

/*位置与访问管理*/
int file_denyWrite(struct file* file);
int file_allowWrite(struct file* file);
// inline bool file_readable(struct file* file);
// inline bool file_writable(struct file* file);

/*状态管理与通知*/
int file_setAccessed(struct file *file);
int file_setModified(struct file *file);

/*标准vfs接口*/
ssize_t file_read(struct file*, char*, size_t, loff_t*);
ssize_t file_write(struct file*, const char*, size_t, loff_t*);
loff_t file_llseek(struct file*, loff_t, int);	
	// pos的变化与查询统一接口,setpos和getpos都支持
int file_sync(struct file*, int);
/* Vectored I/O functions */
ssize_t file_readv(struct file *file, const struct io_vector *vec, 
	unsigned long vlen, loff_t *pos);
	
ssize_t file_writev(struct file *file, const struct io_vector *vec, 
	 unsigned long vlen, loff_t *pos);




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
 * File mode fmode_t flags
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



#endif /* _FILE_H */