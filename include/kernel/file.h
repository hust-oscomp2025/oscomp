#ifndef _FILE_H
#define _FILE_H
#include <kernel/vfs.h>
#include <kernel/types.h>

#include <spike_interface/atomic.h>

typedef struct dentry dentry_t;
typedef unsigned int fmode_t;

struct file *alloc_vfs_file(dentry_t *file_dentry, int readable,
	int writable, int offset);


// data structure of an openned file
struct file {
  dentry_t *f_dentry;
	//const struct file_operations	*f_op;
	spinlock_t		f_lock;
	fmode_t f_mode;
	//struct mutex		f_pos_lock;
  int f_pos;
	// struct file_ra_state	f_ra;
	// struct address_space	*f_mapping;
};



/*
 * flags in file.f_mode.  Note that FMODE_READ and FMODE_WRITE must correspond
 * to O_WRONLY and O_RDWR via the strange trick
 * 这个“strange trick”正是利用了 O_ACCMODE 的值和 FMODE 标志之间的巧妙转换。在
Linux 中，打开文件时的访问模式标志定义为：

O_RDONLY = 0
O_WRONLY = 1
O_RDWR = 2
而在 file.f_mode 中，我们希望用非零的位来表示读和写权限，这里定义：

FMODE_READ = 0x1
FMODE_WRITE = 0x2
这样就希望实现下面的对应关系：

打开为只读的文件：希望 f_mode 为 FMODE_READ (1)
打开为只写的文件：希望 f_mode 为 FMODE_WRITE (2)
打开为读写的文件：希望 f_mode 同时包含读和写，即 (1 | 2 = 3)
但由于 O_RDONLY 为 0，直接赋值会丢失读权限的信息。为了解决这个问题，内核在
do_dentry_open() 中采用了一个简单的“加1”技巧： 将 O_ACCMODE 的值加 1，从而得到：

0 + 1 = 1 → FMODE_READ
1 + 1 = 2 → FMODE_WRITE
2 + 1 = 3 → FMODE_READ | FMODE_WRITE
这样，所有情况都能正确对应，既保证了 O_RDONLY 得到非零的读标志，也使得 O_WRONLY
和 O_RDWR 分别映射到正确的写和读写组合。这个加 1
的“trick”就是用来把内核的打开模式转换为 file.f_mode 中的位标志的关键。 in
do_dentry_open()
 */

/* file is open for reading */
#define FMODE_READ (0x1)
/* file is open for writing */
#define FMODE_WRITE (0x2)
/* file is seekable */
#define FMODE_LSEEK (0x4)
/* file can be accessed using pread */
#define FMODE_PREAD (0x8)
/* file can be accessed using pwrite */
#define FMODE_PWRITE (0x10)
/* File is opened for execution with sys_execve / sys_uselib */
#define FMODE_EXEC (0x20)
/* File is opened with O_NDELAY (only set for block devices) */
#define FMODE_NDELAY (0x40)
/* File is opened with O_EXCL (only set for block devices) */
#define FMODE_EXCL (0x80)
/* File is opened using open(.., 3, ..) and is writeable only for ioctls
   (specialy hack for floppy.c) */
#define FMODE_WRITE_IOCTL (0x100)
/* 32bit hashes as llseek() offset (for directories) */
#define FMODE_32BITHASH (0x200)
/* 64bit hashes as llseek() offset (for directories) */
#define FMODE_64BITHASH (0x400)
/*
 * Don't update ctime and mtime.
 *
 * Currently a special hack for the XFS open_by_handle ioctl, but we'll
 * hopefully graduate it to a proper O_CMTIME flag supported by open(2) soon.
 */
#define FMODE_NOCMTIME (0x800)

/* Expect random access pattern */
#define FMODE_RANDOM (0x1000)

/* File is huge (eg. /dev/mem): treat loff_t as unsigned */
#define FMODE_UNSIGNED_OFFSET (0x2000)

/* File is opened with O_PATH; almost nothing can be done with it */
#define FMODE_PATH (0x4000)

/* File needs atomic accesses to f_pos */
#define FMODE_ATOMIC_POS (0x8000)
/* Write access to underlying fs */
#define FMODE_WRITER (0x10000)
/* Has read method(s) */
#define FMODE_CAN_READ (0x20000)
/* Has write method(s) */
#define FMODE_CAN_WRITE (0x40000)

#define FMODE_OPENED (0x80000)
#define FMODE_CREATED (0x100000)

/* File is stream-like */
#define FMODE_STREAM (0x200000)

/* File supports DIRECT IO */
#define FMODE_CAN_ODIRECT (0x400000)

/* File was opened by fanotify and shouldn't generate fanotify events */
#define FMODE_NONOTIFY (0x4000000)

/* File is capable of returning -EAGAIN if I/O will block */
#define FMODE_NOWAIT (0x8000000)

/* File represents mount that needs unmounting */
#define FMODE_NEED_UNMOUNT (0x10000000)

/* File does not contribute to nr_files count */
#define FMODE_NOACCOUNT (0x20000000)

/* File supports async buffered reads */
#define FMODE_BUF_RASYNC (0x40000000)

/* File supports async nowait buffered writes */
#define FMODE_BUF_WASYNC (0x80000000)


#endif