#ifndef _FILE_H
#define _FILE_H

#include <kernel/fs/vfs/path.h>
#include <kernel/types.h>
#include <kernel/util/atomic.h>
#include <kernel/util/spinlock.h>
#include <kernel/vfs.h>

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
	uint32 f_flags; /* Kernel internal flags */

	/* Memory management */
	// struct addrSpace* f_mapping; /* Page cache mapping */
	//  直接在inode里边

	/* Private data */
	void* f_private; /* Filesystem/driver private data */

	/* Data read-ahead */
	struct file_ra_state {
		uint64 start;      /* Current window start */
		uint64 size;       /* Size of read-ahead window */
		uint64 async_size; /* Async read-ahead size */
		uint32 ra_pages;    /* Maximum pages to read ahead */
		uint32 mmap_miss;   /* Cache miss stat for mmap */
		uint32 prev_pos;
	} f_read_ahead; /* Read-ahead state */

	const struct file_operations* f_operations; /* File s_operations */
};

#define f_mapping f_inode->i_mapping
#define f_dentry f_path.dentry

/*
 * File API functions
 */
/*打开或创建文件*/
struct file* file_open(const char* filename, int32 flags, fmode_t mode);
struct file* file_openPath(const struct path* path, int32 flags, fmode_t mode);

struct file* file_get(struct file* file);
int32 file_put(struct file* file);

/*位置与访问管理*/
int32 file_denyWrite(struct file* file);
int32 file_allowWrite(struct file* file);
// inline bool file_isReadable(struct file* file);
// inline bool file_isWriteable(struct file* file);

/*状态管理与通知*/
int32 file_setAccessed(struct file* file);
int32 file_setModified(struct file* file);

/*标准vfs接口*/
ssize_t file_read(struct file*, char*, size_t, loff_t*);
ssize_t file_write(struct file*, const char*, size_t, loff_t*);
loff_t file_llseek(struct file*, loff_t, int32);
// pos的变化与查询统一接口,setpos和getpos都支持
int32 file_sync(struct file*, int32);
/* Vectored I/O functions */
ssize_t file_readv(struct file* file, const struct io_vector* vec, uint64 vlen, loff_t* pos);

ssize_t file_writev(struct file* file, const struct io_vector* vec, uint64 vlen, loff_t* pos);

static inline bool file_isReadable(struct file* file) {
	if (!file || !file->f_inode || atomic_read(&file->f_refcount) <= 0) return false;
	return (file->f_mode & FMODE_READ) != 0;
}

static inline bool file_isWriteable(struct file* file) {
	if (!file || !file->f_inode || atomic_read(&file->f_refcount) <= 0) return false;
	return (file->f_mode & FMODE_WRITE) != 0;
}

/* 文件预读(readahead)相关常量 */
#define READ_AHEAD_DEFAULT 16    /* 默认预读窗口大小(页) */
#define READ_AHEAD_MAX 128       /* 最大预读页数 */
#define READ_AHEAD_MIN 4         /* 最小预读窗口大小 */
#define READ_AHEAD_ASYNC_RATIO 2 /* 异步预读与同步预读的比例 */

/* 特殊文件类型预读参数 */
#define READ_AHEAD_PIPE         16      /* 管道预读大小 */
#define READ_AHEAD_SOCKET       8       /* 套接字预读大小 */
#define READ_AHEAD_TTY          4       /* 终端预读大小 */







#endif /* _FILE_H */