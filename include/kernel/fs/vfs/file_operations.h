#include <kernel/vfs.h>


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
	int32 (*open)(struct inode*, struct file*);
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
