#ifndef _SCHED_H_
#define _SCHED_H_

#include <kernel/process.h>

//length of a time slice, in number of ticks
#define TIME_SLICE_LEN  2

void insert_to_ready_queue( struct task_struct* proc );
void schedule();

#endif
