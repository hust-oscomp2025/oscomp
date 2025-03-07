#ifndef _PROC_H_
#define _PROC_H_

#include <kernel/riscv.h>
#include <kernel/proc_file.h>


// riscv-pke kernel supports at most 32 processes
#define NPROC 32
// maximum number of pages in a process's heap
#define MAX_HEAP_PAGES 32


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
enum segment_type {
  STACK_SEGMENT = 0,   // runtime stack segmentm from init_user_stack
  CONTEXT_SEGMENT, // trapframe segment, from alloc_process
  SYSTEM_SEGMENT,  // system segment,from alloc_process
  HEAP_SEGMENT,    // runtime heap segment, from init_user_heap
  CODE_SEGMENT,    // ELF segmentm from elf_load_segment
  DATA_SEGMENT,    // ELF segment, from elf_load_segment
};

// types of a segment
enum fork_choice {
  FORK_MAP = 0,   // 直接映射代码段
  FORK_COPY, // 直接复制所有数据
	FORK_COW,
};

// the VM regions mapped to a user process
typedef struct mapped_region {
  uint64 va;       // mapped virtual address
  uint32 npages;   // mapping_info is unused if npages == 0
  uint32 seg_type; // segment type, one of the segment_types
} mapped_region;

typedef struct process_heap_manager {
  // points to the last free page in our simple heap.
  uint64 heap_top;
  // points to the bottom of our simple heap.
  uint64 heap_bottom;

  // the address of free pages in the heap
  // uint64 free_pages_address[MAX_HEAP_PAGES];
  // the number of free pages in the heap
  // uint32 free_pages_count;
}process_heap_manager;

// code file struct, including directory index and file name char pointer
typedef struct {
    uint64 dir; char *file;
} code_file;

// address-line number-file name table
typedef struct {
    uint64 addr, line, file;
} addr_line;

// the extremely simple definition of process, used for begining labs of PKE
typedef struct process_t {
  // pointing to the stack used in trap handling.
  uint64 kstack;
  // user page table
  pagetable_t pagetable;
  // trapframe storing the context of a (User mode) process.
  trapframe* trapframe;

	// lab3_challenge2新增：内核上下文。用来从内核阻塞中恢复。
	trapframe* ktrapframe;

  // added @lab1_challenge2
  char *debugline;
  char **dir;
  code_file *file;
  addr_line *line;
  int line_count;

  // user stack bottom. added @lab2_challenge1
  uint64 user_stack_bottom;

  //heap_block* heap;
  // size_t heap_size;


  // points to a page that contains mapped_regions. below are added @lab3_1
  mapped_region *mapped_info;
  // next free mapped region in mapped_info
  int total_mapped_region;

  // heap management
  process_heap_manager user_heap;

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

// initialize process pool (the procs[] array)
void init_proc_pool();
// allocate an empty process, init its vm space. returns its pid
process* alloc_process();
void init_user_stack(process* ps);
void init_user_heap(process* ps);



// reclaim a process, destruct its vm space and free physical pages.
int free_process( process* proc );
// fork a child from parent
int do_fork(process* parent);
int do_exec(void *path);
ssize_t do_wait(int pid);
// current_percpu points to the currently running user-mode application.
extern process* current_percpu[NCPU];
#define current (current_percpu[read_tp()])
// current_percpu running process
// extern process* current_percpu[NCPU];

#endif
