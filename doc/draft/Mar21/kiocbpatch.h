#ifndef _KIOCB_H
#define KIOCB_NOUPDATE_POS  (1 << 0)   /* Don't update file position */
#define KIOCB_SYNC          (1 << 1)   /* Synchronous I/O */
#define KIOCB_DIRECT        (1 << 2)   /* Direct I/O, bypass page cache */
#define KIOCB_NOWAIT        (1 << 3)   /* Don't block on locks or I/O */
#define KIOCB_APPEND        (1 << 4)   /* File is opened in append mode */
#define KIOCB_CACHED        (1 << 5)   /* Use page cache for I/O */
#define KIOCB_HIGH_PRIO     (1 << 6)   /* High priority I/O */
#define KIOCB_CANCELABLE    (1 << 7)   /* I/O can be canceled */
#define KIOCB_VECTORED      (1 << 8)   /* Vectored I/O operation */

/* Operation codes for I/O */
#define KIOCB_OP_READ       0
#define KIOCB_OP_WRITE      1
#define KIOCB_OP_SYNC       2
#define KIOCB_OP_FSYNC      3

/* I/O priority levels */
#define KIOCB_PRIO_IDLE     0  /* Background, lowest priority */
#define KIOCB_PRIO_BE       1  /* Best effort, default */
#define KIOCB_PRIO_RT       2  /* Real-time, highest priority */ _KIOCB_H

#include <kernel/types.h>
#include <kernel/fs/io_vector.h>
#include <kernel/time.h>

struct file;
struct kiocb_aio_context;
struct kiocb_throttle;
struct kiocb_prio;
struct kiocb_stats;
struct kiocb_error;
struct kiocb_cache;
struct kiocb_cancelable;

/* Forward declarations for callback types */
typedef void (*ioCompletion_callback)(struct kiocb *, long);

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
    int ki_opcode;             /* Operation code (READ/WRITE/etc) */
    int ki_prio;               /* I/O priority */
    struct kiocb_error *ki_error; /* Error tracking */
    struct kiocb_cache *ki_cache; /* Cache for I/O */
    struct timespec ki_start;  /* Start time for operation */
    unsigned long ki_timeout;  /* Timeout for operation */
};

/* KI_OCB flags - used to control I/O behavior */
#define KIOCB_NOUPDATE_POS  (1 << 0)   /* Don't update file position */
#define KIOCB_SYNC          (1 << 1)   /* Synchronous I/O */
#define