#include <kernel/fs/vfs/vfs.h>


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
	ssize_t (*read_iter)(struct kiocb*, struct io_vector_iterator*);
	ssize_t (*write_iter)(struct kiocb*, struct io_vector_iterator*);

	/* Directory s_operations */
	int (*iterate)(struct file*, struct dir_context*);
	int (*iterate_shared)(struct file*, struct dir_context*);

	/* Polling/selection */
	__poll_t (*poll)(struct file*, struct poll_table_struct*);

	/* Management s_operations */
	int (*open)(struct inode*, struct file*);
	int (*flush)(struct file*);
	int (*release)(struct inode*, struct file*);
	int (*fsync)(struct file*, loff_t, loff_t, int datasync);

	/* Memory mapping */
	int (*mmap)(struct file*, struct vm_area_struct*);

	/* Special s_operations */
	long (*unlocked_ioctl)(struct file*, unsigned int, unsigned long);
	int (*fasync)(int, struct file*, int);

	/* Splice s_operations */
	ssize_t (*splice_read)(struct file*, loff_t*, struct pipe_inode_info*, size_t, unsigned int);
	ssize_t (*splice_write)(struct pipe_inode_info*, struct file*, loff_t*, size_t, unsigned int);

	/* Space allocation */
	long (*fallocate)(struct file*, int, loff_t, loff_t);
};
