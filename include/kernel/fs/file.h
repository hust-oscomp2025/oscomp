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

typedef uint64 loff_t;
typedef uint32 fmode_t;
typedef unsigned int __poll_t;

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

  /* Directory operations */
  int (*iterate)(struct file*, struct dir_context*);
  int (*iterate_shared)(struct file*, struct dir_context*);

  /* Polling/selection */
  __poll_t (*poll)(struct file*, struct poll_table_struct*);

  /* Management operations */
  int (*open)(struct inode*, struct file*);
  int (*flush)(struct file*);
  int (*release)(struct inode*, struct file*);
  int (*fsync)(struct file*, loff_t, loff_t, int datasync);

  /* Memory mapping */
  int (*mmap)(struct file*, struct vm_area_struct*);

  /* Special operations */
  long (*unlocked_ioctl)(struct file*, unsigned int, unsigned long);
  int (*fasync)(int, struct file*, int);

  /* Splice operations */
  ssize_t (*splice_read)(struct file*, loff_t*, struct pipe_inode_info*, size_t,
                         unsigned int);
  ssize_t (*splice_write)(struct pipe_inode_info*, struct file*, loff_t*,
                          size_t, unsigned int);

  /* Space allocation */
  long (*fallocate)(struct file*, int, loff_t, loff_t);
};

/**
 * Represents an open file in the system
 */
struct file {
  /* File identity */
  struct dentry* f_dentry;            /* Associated dentry */
  struct inode* f_inode;              /* Inode of the file */
  struct path f_path;                 /* Path to file */
  const struct file_operations* f_op; /* File operations */

  /* File state */
  fmode_t f_mode;       /* File access mode */
  spinlock_t f_lock;    /* Lock for f_flags/f_pos */
  atomic_t f_count;     /* Reference count */
  unsigned int f_flags; /* Kernel internal flags */

  /* File position */
  loff_t f_pos; /* Current file position */

  /* Memory management */
  struct address_space* f_mapping; /* Page cache mapping */

  /* Private data */
  void* f_private; /* Filesystem/driver private data */

  /* Data read-ahead */
  struct file_ra_state f_ra; /* Read-ahead state */
};

/*
 * File API functions
 */
struct file* file_open_path(const struct path* path, int flags, mode_t mode);
struct file* file_open_qstr(const struct qstr* name, int flags, mode_t mode);
struct file* file_open(const char* filename, int flags, mode_t mode);

/**
 * File descriptor table structure
 */
struct fd_struct {
  struct file** fd_array; /* Array of file pointers */
  unsigned int* fd_flags; /* Array of file pointers */

  unsigned int max_fds; /* Size of the array */
  unsigned int next_fd; /* Next free fd number */
  spinlock_t file_lock; /* Lock for the struct */
  atomic_t count;       /* Reference count */
};

/* Process-level file table management */
int init_files(void);
struct fd_struct* get_files_struct(struct task_struct* task);
void put_files_struct(struct fd_struct* files);
int unshare_files(struct fd_struct** new_filesp,
                  struct fd_struct* old_files);

/* File descriptor management */
struct file* get_file(unsigned int fd, struct task_struct* owner);
void put_file(struct file* file, struct task_struct* owner);

int alloc_fd(struct file* file, unsigned int flags);


/**
 * Read-ahead state for file
 */
struct file_ra_state {
  unsigned long start;      /* Current window start */
  unsigned long size;       /* Size of read-ahead window */
  unsigned long async_size; /* Async read-ahead size */
  unsigned int ra_pages;    /* Maximum pages to read ahead */
  unsigned int mmap_miss;   /* Cache miss stat for mmap */
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
#define O_RSYNC 00040000           /* Synchronize read operations */
#define O_DIRECT 00100000          /* Direct I/O */
#define O_DIRECTORY 00200000       /* Must be a directory */
#define O_NOFOLLOW 00400000        /* Don't follow symbolic links */
#define O_CLOEXEC 02000000         /* Close on exec */
#define O_PATH 010000000           /* Path-only access */









/* File reading and writing */
ssize_t kernel_read(struct file* file, void* buf, size_t count, loff_t* pos);
ssize_t kernel_write(struct file* file, const void* buf, size_t count,
                     loff_t* pos);


#endif /* _FILE_H */