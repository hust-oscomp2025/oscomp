/*
 * implementing the scheduler
 */

#include <kernel/mm/kmalloc.h>
#include <kernel/sched.h>
#include <kernel/trapframe.h>
#include <spike_interface/spike_utils.h>

struct task_struct *ready_queue = NULL;
struct task_struct* procs[NPROC];
struct task_struct* current_percpu[NCPU];

struct task_struct *alloc_empty_process() {
  for (int i = 0; i < NPROC; i++) {
    if (procs[i] == NULL) {
			procs[i] = (struct task_struct*)kmalloc(sizeof(struct task_struct));
			memset(procs[i],0,sizeof(struct task_struct));
      return procs[i];
    }
  }
  panic("cannot find any free process structure.\n");
}
//
// insert a process, proc, into the END of ready queue.
//
void insert_to_ready_queue(struct task_struct *proc) {
  sprint("going to insert process %d to ready queue.\n", proc->pid);
  // if the queue is empty in the beginning
  if (ready_queue == NULL) {
    // sprint("ready_queue is empty\n");
    proc->queue_next = NULL;
    ready_queue = proc;
    return;
  }
  // ready queue is not empty
  struct task_struct *p;
  // browse the ready queue to see if proc is already in-queue
  for (p = ready_queue; p->queue_next != NULL; p = p->queue_next)
    if (p == proc)
      return; // already in queue
  // p points to the last element of the ready queue
  if (p == proc)
    return;
  p->queue_next = proc;
  proc->queue_next = NULL;
  return;
}

//
// initialize process pool (the procs[] array). added @lab3_1
//
void init_scheduler() {
	pid_init();
  memset(procs, 0, sizeof(struct task_struct*) * NPROC);

  for (int i = 0; i < NPROC; ++i) {
    procs[i] = NULL;
  }
	sprint("Process pool initiated\n");

}


//
// choose a proc from the ready queue, and put it to run.
// note: schedule() does not take care of previous current process. If the
// current process is still runnable, you should place it into the ready queue
// (by calling ready_queue_insert), and then call schedule().
//
void schedule() {
  extern struct task_struct *procs[NPROC];
  int hartid = read_tp();
  struct task_struct *cur = CURRENT;
  // sprint("debug\n");
  if (cur &&
      (cur->status == TASK_INTERRUPTIBLE |
       cur->status == TASK_UNINTERRUPTIBLE) &&
      cur->ktrapframe == NULL) {
    cur->ktrapframe = (struct trapframe *)kmalloc(sizeof(struct trapframe));
    store_all_registers(cur->ktrapframe);
    // sprint("cur->ktrapframe->regs.ra=0x%x\n",cur->ktrapframe->regs.ra);
  }

  if (!ready_queue) {
    // by default, if there are no ready process, and all processes are in the
    // status of FREE and ZOMBIE, we should shutdown the emulated RISC-V
    // machine.
    int should_shutdown = 1;

    for (int i = 0; i < NPROC; i++)
      if ((procs[i]) && (procs[i]->exit_state != EXIT_TRACE)) {
        should_shutdown = 0;
        sprint("ready queue empty, but process %d is not in free/zombie "
               "state:%d\n",
               i, procs[i]->exit_state);
      }

    // 如果所有进程都在 FREE 或 ZOMBIE 状态，允许关机
    if (should_shutdown) {
      // 确保只有 hartid == 0 的核心执行 shutdown
      if (hartid == 0) {
        sprint("no more ready processes, system shutdown now.\n");
        shutdown(0); // 只有核心 0 执行关机
      }
      // 否则，其他核心等待关机标志
      else {
        while (1) {
          // 自旋等待，直到 hartid == 0 核心完成 shutdown
        }
      }
    } else {
      panic(
          "Not handled: we should let system wait for unfinished processes.\n");
    }
  }

  cur = ready_queue;
  ready_queue = ready_queue->queue_next;
  CURRENT = cur;
  if (cur->ktrapframe != NULL) {
    restore_all_registers(cur->ktrapframe);
    kfree(cur->ktrapframe);
    cur->ktrapframe = NULL;
    return;
  }

  sprint("going to schedule process %d to run.\n", cur->pid);
  switch_to(cur);
}
