// Add to include/kernel/fs/writeback.h
#ifndef _WRITEBACK_H
#define _WRITEBACK_H

#include <kernel/types.h>

/*
 * Writeback flags
 */
#define WB_SYNC_NONE    0       /* Don't wait on completion */
#define WB_SYNC_ALL     1       /* Wait on all write completion */
/**
 * Reason for writeback
 */
enum wb_reason {
    WB_REASON_BACKGROUND,        /* Regular background writeback */
    WB_REASON_SYNC,              /* Explicit sync operation */
    WB_REASON_PERIODIC,          /* Periodic flush */
    WB_REASON_VMSCAN,            /* Memory pressure */
    WB_REASON_SHUTDOWN           /* System shutdown */
};

/**
 * Controls writeback operations for dirty pages/buffers
 */
struct writeback_control {
    int64 nr_to_write;           /* Number of pages to write */
    int64 pages_skipped;         /* Pages skipped because they weren't dirty */
    
    /* Writeback range */
    loff_t range_start;         /* Start offset for writeback */
    loff_t range_end;           /* End offset for writeback */
    
    /* Flags */
    uint32 for_kupdate:1; /* Operation for kupdate functionality */
    uint32 for_background:1; /* Background operation */
    uint32 for_reclaim:1;   /* Page reclaim writeback */
    uint32 range_cyclic:1;  /* Range is cyclic */
    uint32 sync_mode:1;     /* Sync mode (WB_SYNC_ALL or WB_SYNC_NONE) */
    
    /* In case we need other flags in the future */
    uint32 more_io:1;      /* More IO to follow */
    uint32 no_cgroup_owner:1; /* Don't cgroup this writeback */
    uint32 punt_to_cgroup:1;  /* Cgroup should do this writeback */
    
    /* For tracking which process initiated writeback */
    uid_t uid;                   /* Owner UID of writeback task */
    
    /* Reason for writeback */
    enum wb_reason reason;        /* Why writeback was triggered */
};


/* Core writeback functions */
void writeback_inodes_sb(struct superblock *, enum wb_reason);
int64 writeback_inodes_s_if_idle(struct superblock *, enum wb_reason);
int32 sync_filesystem(struct superblock *);
void init_writeback_control(struct writeback_control *wbc, uint32 sync_mode);

#endif /* _WRITEBACK_H */