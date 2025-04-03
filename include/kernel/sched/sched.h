#ifndef _SCHED_H_
#define _SCHED_H_

#include <kernel/sched/process.h>
// riscv-pke kernel supports at most 32 processes
#define NPROC 32
#define TIME_SLICE_LEN  2
extern struct task_struct* current_percpu[NCPU];
#define CURRENT (current_percpu[read_tp()])
#define current current_task()

static inline struct task_struct* current_task() {
		return CURRENT;
}

void init_scheduler();
void insert_to_ready_queue( struct task_struct* proc );
struct task_struct *alloc_empty_process();

void switch_to(struct task_struct*);

void schedule();
struct task_struct *find_process_by_pid(pid_t pid);

/**
 * set_current_task - Explicitly set the current task for the calling CPU
 * @task: Task to set as current
 * 
 * Sets the specified task as the current task for the CPU core
 * that called this function. This is primarily used during
 * initialization or special operations like setting up the init task.
 */
static inline void set_current_task(struct task_struct* task) {
    int32 hartid = read_tp();
    current_percpu[hartid] = task;
}


#endif
