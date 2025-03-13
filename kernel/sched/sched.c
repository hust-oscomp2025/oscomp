/*
 * implementing the scheduler
 */

#include <kernel/mm/kmalloc.h>
#include <kernel/mm/mm_struct.h>
#include <kernel/sched/pid.h>
#include <kernel/sched/sched.h>
#include <kernel/trapframe.h>
#include <spike_interface/spike_utils.h>
#include <util/list.h>
#include <util/string.h>

struct list_head ready_queue;
struct task_struct *procs[NPROC];
struct task_struct *current_percpu[NCPU];

//
// initialize process pool (the procs[] array). added @lab3_1
//
void init_scheduler() {
	//sprint("init_scheduler: start\n");
  INIT_LIST_HEAD(&ready_queue);
  pid_init();
  memset(procs, 0, sizeof(struct task_struct *) * NPROC);

  for (int i = 0; i < NPROC; ++i) {
    procs[i] = NULL;
  }
  sprint("Scheduler initiated\n");
}

struct task_struct *alloc_empty_process() {
  for (int i = 0; i < NPROC; i++) {
    if (procs[i] == NULL) {
      procs[i] = (struct task_struct *)kmalloc(sizeof(struct task_struct));
      memset(procs[i], 0, sizeof(struct task_struct));
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
  list_add(&proc->queue_node, &ready_queue);
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
      ((cur->state == TASK_INTERRUPTIBLE) |
       (cur->state == TASK_UNINTERRUPTIBLE)) &&
      cur->ktrapframe == NULL) {
    cur->ktrapframe = (struct trapframe *)kmalloc(sizeof(struct trapframe));
    store_all_registers(cur->ktrapframe);
    // sprint("cur->ktrapframe->regs.ra=0x%x\n",cur->ktrapframe->regs.ra);
  }
  if (list_empty(&ready_queue)) {
    // by default, if there are no ready process, and all processes are in the
    // state of FREE and ZOMBIE, we should shutdown the emulated RISC-V
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

  CURRENT = container_of(ready_queue.next, struct task_struct, queue_node);
  list_del_init(ready_queue.next);
  if (cur->ktrapframe != NULL) {
    sprint("going to schedule process %d to run in s-mode.\n", CURRENT->pid);

    restore_all_registers(cur->ktrapframe);
    kfree(cur->ktrapframe);
    cur->ktrapframe = NULL;
    return;
  } else {
    sprint("going to schedule process %d to run in u-mode.\n", CURRENT->pid);
    switch_to(cur);
  }
}

void switch_to(struct task_struct *proc) {

  assert(proc);
  CURRENT = proc;

  extern char smode_trap_vector[];
  write_csr(stvec, (uint64)smode_trap_vector);
  // set up trapframe values (in process structure) that smode_trap_vector will
  // need when the process next re-enters the kernel.
  proc->trapframe->kernel_sp = proc->kstack;     // process's kernel stack
  proc->trapframe->kernel_satp = read_csr(satp); // kernel page table


	extern char  smode_trap_handler[];
  proc->trapframe->kernel_trap = (uint64)smode_trap_handler;
  proc->trapframe->kernel_schedule = (uint64)schedule;

  // SSTATUS_SPP and SSTATUS_SPIE are defined in kernel/riscv.h
  // set S Previous Privilege mode (the SSTATUS_SPP bit in sstatus register) to
  // User mode,to to enable interrupts, and sret destination.

  write_csr(sstatus, ((read_csr(sstatus) & ~SSTATUS_SPP) | SSTATUS_SPIE));

  // set S Exception Program Counter (sepc register) to the elf entry pc.
  write_csr(sepc, proc->trapframe->epc);
  sprint("return to user\n");


	extern void return_to_user(struct trapframe*, uint64);
  return_to_user(proc->trapframe, MAKE_SATP(proc->mm->pagetable));
}
