#ifndef _SCHED_H_
#define _SCHED_H_

#include <kernel/sched/process.h>
// riscv-pke kernel supports at most 32 processes
#define NPROC 32
#define TIME_SLICE_LEN  2
extern struct task_struct* current_percpu[NCPU];
#define CURRENT (current_percpu[read_tp()])

inline struct task_struct* current_task() {
		return CURRENT;
}

void init_scheduler();
void insert_to_ready_queue( struct task_struct* proc );
struct task_struct *alloc_empty_process();

void switch_to(struct task_struct*);

void schedule();

#endif
