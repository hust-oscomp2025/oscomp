#ifndef _PROC_H_
#define _PROC_H_

#include <kernel/riscv.h>
#include <kernel/proc_file.h>




// riscv-pke kernel supports at most 32 processes
#define NPROC 32


typedef struct trapframe_t {
  // space to store context (all common registers)
  /* offset:0   */ riscv_regs regs;

  // process's "user kernel" stack
  /* offset:248 */ uint64 kernel_sp;
  // pointer to smode_trap_handler
  /* offset:256 */ uint64 kernel_trap;
  // saved user process counter
  /* offset:264 */ uint64 epc;

  // kernel page table. added @lab2_1
  /* offset:272 */ uint64 kernel_satp;
	// kernel scheduler, added @lab3_challenge2
	/* offset:280 */ uint64 kernel_schedule;
}trapframe;



// possible status of a process
enum proc_status {
  FREE,            // unused state
  READY,           // ready state
  RUNNING,         // currently running
  BLOCKED,         // waiting for something
  ZOMBIE,          // terminated but not reclaimed yet
};

// types of a segment
enum fork_choice {
  FORK_MAP = 0,   // 直接映射代码段
  FORK_COPY, // 直接复制所有数据
	FORK_COW,
};


// the extremely simple definition of process, used for begining labs of PKE
typedef struct process_t {
  uint64 kstack;		// 分配一个页面当内核栈，注意内核栈的范围是[kstack-PGSIZE, kstack)
  trapframe* trapframe;
	trapframe* ktrapframe;

  struct mm_struct *mm;

  // heap management
  // process_heap_manager user_heap;

  // process id
  uint64 pid;
  // process status
  int status;
  // parent process
  struct process_t *parent;
  // next queue element
  struct process_t *queue_next;

  // accounting. added @lab3_3
  int tick_count;

	int sem_index;
  // file system. added @lab4_1
  proc_file_management *pfiles;

}process;

// switch to run user app
void switch_to(process*);
void init_proc_pool();
process* alloc_process();
int free_process( process* proc );

// fork a child from parent
int do_fork(process* parent);
int do_exec(void *path);
ssize_t do_wait(int pid);
// current_percpu points to the currently running user-mode application.

// current_percpu running process
// extern process* current_percpu[NCPU];
extern process* current_percpu[NCPU];
#define CURRENT (current_percpu[read_tp()])

#endif
