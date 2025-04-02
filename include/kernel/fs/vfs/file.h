#ifndef _FILE_H
#define _FILE_H

#include <kernel/fs/vfs/path.h>
#include "forward_declarations.h"
#include <kernel/util.h>
#include <kernel/types.h>

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
	union{
		struct path f_path;    /* Path to file */
		struct {
            struct dentry* f_dentry; /* Dentry of the file */
			struct vfsmount* f_mnt;  /* Mount point of the file */
		};
	};
	
	struct inode* f_inode; /* Inode of the file */

	/* File state */
	fmode_t f_mode;       /* File access mode */
	loff_t f_pos;         /* Current file position */
	uint32 f_flags; /* Kernel internal flags */


	/* Private data */
	void* f_private; /* Filesystem/driver private data */

	const struct file_operations* f_op; /* File s_operations */
};

/**
 * Directory context for readdir operations
 */
struct dir_context {
	int32 (*actor)(struct dir_context*, const char*, int32, loff_t, uint64_t,
		     unsigned);
	loff_t pos; /* Current position in directory */
};

/**
 * File operation structure - provides methods for file manipulation
 */
struct file_operations {
	int32 (*open)(struct file * file);

	/* Position manipulation */
	loff_t (*llseek)(struct file*, loff_t, int32);

	/* Basic I/O */
	ssize_t (*read)(struct file*, char*, size_t, loff_t*);
	ssize_t (*write)(struct file*, const char*, size_t, loff_t*);

	/* Vectored I/O */
	ssize_t (*read_iter)(struct kiocb*, struct io_vector_iterator*);
	ssize_t (*write_iter)(struct kiocb*, struct io_vector_iterator*);

	/* Directory s_operations */
	int32 (*iterate)(struct file*, struct dir_context*);
	int32 (*iterate_shared)(struct file*, struct dir_context*);

	/* Polling/selection */
	//__poll_t (*poll)(struct file*, struct poll_table_struct*);

	/* Management s_operations */

	int32 (*flush)(struct file*);
	int32 (*release)(struct inode*, struct file*);
	int32 (*fsync)(struct file*, loff_t, loff_t, int32 datasync);

	/* Memory mapping */
	int32 (*mmap)(struct file*, struct vm_area_struct*);

	/* Special s_operations */
	int64 (*unlocked_ioctl)(struct file*, uint32, uint64);
	int32 (*fasync)(int32, struct file*, int32);

	/* Splice s_operations */
	//ssize_t (*splice_read)(struct file*, loff_t*, struct pipe_inode_info*, size_t, uint32);
	//ssize_t (*splice_write)(struct pipe_inode_info*, struct file*, loff_t*, size_t, uint32);

	/* Space allocation */
	int64 (*fallocate)(struct file*, int32, loff_t, loff_t);
};

/*
 * File API functions
 */
/*file syscall functions*/
int32 file_open(struct file* file);




/*file syscall functions*/

int32 file_close(struct file* file);

struct file* file_ref(struct file* file);
int32 file_unref(struct file* file);

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

int32 iterate_dir(struct file*, struct dir_context*);


bool file_isReadable(struct file* file);
bool file_isWriteable(struct file* file);



/*file_open系统调用*/
/* 有效的打开标志掩码 - 添加到头文件 */
#define VALID_OPEN_FLAGS ( \
    O_ACCMODE | O_CREAT | O_EXCL | O_TRUNC | O_APPEND | \
    O_NONBLOCK | O_SYNC | O_DIRECT | O_DIRECTORY | \
    O_NOFOLLOW | O_NOATIME | O_CLOEXEC | O_PATH | \
    O_DSYNC | O_ASYNC | O_EXEC \
)
fmode_t open_flags_to_fmode(int32 flags);
int32 validate_open_flags(int32 flags);
/*
 * 文件模式(FMODE)定义
 * 这些模式定义了内核对文件的访问权限和操作行为
 */

/* 基本访问模式 */
#define FMODE_READ           (1 << 0)    /* 文件可读 */
#define FMODE_WRITE          (1 << 1)    /* 文件可写 */
#define FMODE_EXEC           (1 << 2)    /* 文件可执行 */

/* 打开/访问行为 */
#define FMODE_APPEND         (1 << 3)    /* 追加模式写入 */
#define FMODE_NONBLOCK       (1 << 4)    /* 非阻塞I/O */
#define FMODE_DIRECT         (1 << 5)    /* 直接I/O，绕过页缓存 */
#define FMODE_SYNC           (1 << 6)    /* 同步I/O操作 */
#define FMODE_EXCL           (1 << 7)    /* 独占访问 */
#define FMODE_NDELAY         FMODE_NONBLOCK /* 别名 */

/* 特殊文件访问模式 */
#define FMODE_RANDOM         (1 << 8)    /* 随机访问（影响预读算法） */
#define FMODE_PREAD          (1 << 9)    /* 支持pread/pwrite系统调用 */
#define FMODE_ATOMIC_POS     (1 << 10)   /* 原子位置更新 */

/* 目录相关模式 */
#define FMODE_DIRECTORY      (1 << 11)   /* 文件是目录 */
#define FMODE_PATH           (1 << 12)   /* 只关心路径，不关心内容 */
#define FMODE_NOKERNFSPATH   (1 << 13)   /* 内核不应构建路径 */

/* 内部标志 */
#define FMODE_LSEEK          (1 << 14)   /* 允许lseek操作 */
#define FMODE_CAN_WRITE      (1 << 15)   /* 可以执行物理写入 */
#define FMODE_OPENED         (1 << 16)   /* 文件已经正确打开 */
#define FMODE_CREATED        (1 << 17)   /* 文件是新创建的 */

/* 特殊I/O行为 */
#define FMODE_WRITER         (1 << 18)   /* 独占写入者 */
#define FMODE_NONOTIFY       (1 << 19)   /* 不发送文件修改通知 */
#define FMODE_NOACCOUNT      (1 << 20)   /* I/O操作不计入配额 */
#define FMODE_NOSETLEASE     (1 << 21)   /* 不允许设置租约 */

/* 内存映射相关 */
#define FMODE_MMAP           (1 << 22)   /* 文件被映射到内存 */
#define FMODE_MMAP_SHARED    (1 << 23)   /* 共享内存映射 */

/* 杂项模式 */
#define FMODE_CRYPT          (1 << 24)   /* 加密访问 */
#define FMODE_VERIFY         (1 << 25)   /* 校验访问 */
#define FMODE_BACKUP         (1 << 26)   /* 备份操作，可能绕过某些限制 */
#define FMODE_SIGNED         (1 << 27)   /* 文件内容已签名 */
#define FMODE_KERNEL         (1 << 28)   /* 内核内部使用 */
#define FMODE_SEARCH         (1 << 29)   /* 目录搜索操作 */
#define FMODE_DELETED        (1 << 30)   /* 文件已标记为删除 */

/* 常用组合 */
#define FMODE_RDWR           (FMODE_READ | FMODE_WRITE)  /* 读写模式 */
#define FMODE_EXEC_ONLY      (FMODE_EXEC)               /* 仅执行模式 */
/*file_open系统调用*/



#endif /* _FILE_H */