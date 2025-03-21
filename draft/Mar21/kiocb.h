#ifndef _KIOCB_H
#define _KIOCB_H

#include <kernel/types.h>

struct file;

/**
 * struct io_vector - Describes a memory buffer for vectored I/O
 * @iov_base: Starting address of buffer
 * @iov_len: Size of buffer in bytes
 */
struct io_vector {
    void *iov_base;      /* Starting address */
    size_t iov_len;      /* Number of bytes to transfer */
};

/**
 * struct io_vector_iterator - Iterator for working with I/O vectors
 */
struct io_vector_iterator {
    struct io_vector *iov;   /* Current io_vector */
    unsigned long nr_segs;  /* Number of segments */
    size_t iov_offset;   /* Offset within current io_vector */
    size_t count;        /* Total bytes remaining */
};

/**
 * struct kiocb - Kernel I/O control block
 * Used for both synchronous and asynchronous I/O
 */
struct kiocb {
    struct file *ki_filp;      /* File for the I/O */
    loff_t ki_pos;             /* Current file position */
    void (*ki_complete)(struct kiocb *, long);  /* I/O completion handler */
    void *private;             /* Private data for completion handler */
    int ki_flags;              /* Flags for I/O */
};

/* Vectored I/O functions */
ssize_t vfs_readv(struct file *file, const struct io_vector *vec, 
                  unsigned long vlen, loff_t *pos);
                  
ssize_t vfs_writev(struct file *file, const struct io_vector *vec, 
                   unsigned long vlen, loff_t *pos);

/* Initialize/setup functions */
void init_kiocb(struct kiocb *kiocb, struct file *file);
int setup_io_vector_iterator(struct io_vector_iterator *iter, const struct io_vector *vec, 
                   unsigned long vlen);

#endif /* _KIOCB_H */