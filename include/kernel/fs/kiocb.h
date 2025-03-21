#ifndef _KIOCB_H
#define _KIOCB_H

#include <kernel/types.h>

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



/* Initialize/setup functions */
void init_kiocb(struct kiocb *kiocb, struct file *file);


#endif /* _KIOCB_H */