#ifndef _KIOCB_H
#define _KIOCB_H

#include <kernel/types.h>
#include <kernel/fs/io_vector.h>

struct file;


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



// Initialization and setup
void init_kiocb(struct kiocb *kiocb, struct file *file);
void kiocb_set_pos(struct kiocb *kiocb, loff_t pos);
void kiocb_set_flags(struct kiocb *kiocb, int flags);

// Completion handling
void kiocb_set_completion(struct kiocb *kiocb, void (*complete)(struct kiocb *, long), void *private);
void kiocb_complete(struct kiocb *kiocb, long result);

// State management
int kiocb_is_sync(const struct kiocb *kiocb);
int kiocb_is_async(const struct kiocb *kiocb);
int kiocb_is_error(const struct kiocb *kiocb);

// I/O operations
ssize_t kiocb_read(struct kiocb *kiocb, char *buf, size_t count);
ssize_t kiocb_write(struct kiocb *kiocb, const char *buf, size_t count);
ssize_t kiocb_read_iter(struct kiocb *kiocb, struct io_vector_iterator *iter);
ssize_t kiocb_write_iter(struct kiocb *kiocb, struct io_vector_iterator *iter);


#endif /* _KIOCB_H */