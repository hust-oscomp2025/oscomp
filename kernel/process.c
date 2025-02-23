/*
 * Utility functions for process management.
 *
 * Note: in Lab1, only one process (i.e., our user application) exists.
 * Therefore, PKE OS at this stage will set "current" to the loaded user
 * application, and also switch to the old "current" process after trap
 * handling.
 */

#include "process.h"
#include "config.h"
#include "elf.h"
#include "global.h"
#include "memlayout.h"
#include "pmm.h"
#include "riscv.h"
#include "sched.h"
#include "spike_interface/spike_utils.h"
#include "strap.h"
#include "string.h"
#include "vmm.h"

// moved to global.h
//
// extern char smode_trap_vector[];
// extern void return_to_user(trapframe *, uint64 satp);
// extern char trap_sec_start[];

// moved to global.c
// process procs[NPROC];
// process* current[NCPU];

//
// switch to a user-mode process
//
void switch_to(process *proc) {
  int hartid = read_tp();

  assert(proc);
  current[hartid] = proc;
  write_csr(stvec, (uint64)smode_trap_vector);

  // set up trapframe values (in process structure) that smode_trap_vector will
  // need when the process next re-enters the kernel.
  proc->trapframe->kernel_sp = proc->kstack;     // process's kernel stack
  proc->trapframe->kernel_satp = read_csr(satp); // kernel page table
  proc->trapframe->kernel_trap = (uint64)smode_trap_handler;

  // SSTATUS_SPP and SSTATUS_SPIE are defined in kernel/riscv.h
  // set S Previous Privilege mode (the SSTATUS_SPP bit in sstatus register) to
  // User mode.
  unsigned long x = read_csr(sstatus);
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode

  // write x back to 'sstatus' register to enable interrupts, and sret
  // destination mode.
  write_csr(sstatus, x);

  // set S Exception Program Counter (sepc register) to the elf entry pc.
  write_csr(sepc, proc->trapframe->epc);

  // make user page table. macro MAKE_SATP is defined in kernel/riscv.h. added
  // @lab2_1
  uint64 user_satp = MAKE_SATP(proc->pagetable);
  // return_to_user() is defined in kernel/strap_vector.S. switch to user mode
  // with sret. note, return_to_user takes two parameters @ and after lab2_1.
  return_to_user(proc->trapframe, user_satp);
}

//
// initialize process pool (the procs[] array). added @lab3_1
//
void init_proc_pool() {
  memset(procs, 0, sizeof(process) * NPROC);

  for (int i = 0; i < NPROC; ++i) {
    procs[i].status = FREE;
    procs[i].pid = i;
  }
}

//
// allocate an empty process, init its vm space. returns the pointer to
// process strcuture. added @lab3_1
//
process *alloc_process() {
  // locate the first usable process structure
  process *ps = NULL;
  for (int i = 0; i < NPROC; i++) {
    if (procs[i].status == FREE) {
      ps = &(procs[i]);
      break;
    }
  }
  if (ps == NULL) {
    panic("cannot find any free process structure.\n");
    return 0;
  }

  // init proc[i]'s vm space
  ps->trapframe = (trapframe *)Alloc_page(); // trapframe, used to save context
  //memset(ps->trapframe, 0, sizeof(trapframe));

  // page directory
  ps->pagetable = (pagetable_t)Alloc_page();
  //memset((void *)ps->pagetable, 0, PGSIZE);

  ps->kstack = (uint64)Alloc_page() + PGSIZE; // user kernel stack top
  uint64 user_stack =
      (uint64)Alloc_page(); // phisical address of user stack bottom
  ps->trapframe->regs.sp = USER_STACK_TOP; // virtual address of user stack top

  // allocates a page to record memory regions (segments)
  ps->mapped_info = (mapped_region *)Alloc_page();
  //memset(ps->mapped_info, 0, PGSIZE);

	ps->sem_index = sem_new(0,ps->pid);




  // map user stack in userspace
  user_vm_map((pagetable_t)ps->pagetable, USER_STACK_TOP - PGSIZE, PGSIZE,
              user_stack, prot_to_type(PROT_WRITE | PROT_READ, 1));
  ps->mapped_info[STACK_SEGMENT].va = USER_STACK_TOP - PGSIZE;
  ps->mapped_info[STACK_SEGMENT].npages = 1;
  ps->mapped_info[STACK_SEGMENT].seg_type = STACK_SEGMENT;

  // map trapframe in user space (direct mapping as in kernel space).
  user_vm_map((pagetable_t)ps->pagetable, (uint64)ps->trapframe, PGSIZE,
              (uint64)ps->trapframe, prot_to_type(PROT_WRITE | PROT_READ, 0));
  ps->mapped_info[CONTEXT_SEGMENT].va = (uint64)ps->trapframe;
  ps->mapped_info[CONTEXT_SEGMENT].npages = 1;
  ps->mapped_info[CONTEXT_SEGMENT].seg_type = CONTEXT_SEGMENT;

  // map S-mode trap vector section in user space (direct mapping as in kernel
  // space) we assume that the size of usertrap.S is smaller than a page.
  user_vm_map((pagetable_t)ps->pagetable, (uint64)trap_sec_start, PGSIZE,
              (uint64)trap_sec_start, prot_to_type(PROT_READ | PROT_EXEC, 0));
  ps->mapped_info[SYSTEM_SEGMENT].va = (uint64)trap_sec_start;
  ps->mapped_info[SYSTEM_SEGMENT].npages = 1;
  ps->mapped_info[SYSTEM_SEGMENT].seg_type = SYSTEM_SEGMENT;

  sprint(
      "in alloc_proc. user frame 0x%lx, user stack 0x%lx, user kstack 0x%lx \n",
      ps->trapframe, ps->trapframe->regs.sp, ps->kstack);



  // initialize the process's heap manager
  ps->user_heap.heap_top = USER_FREE_ADDRESS_START;
  ps->user_heap.heap_bottom = USER_FREE_ADDRESS_START;
  // ps->user_heap.free_pages_address = USER_FREE_ADDRESS_START;
  // ps->user_heap.free_pages_count = 0;

  // map user heap in userspace
  ps->mapped_info[HEAP_SEGMENT].va = USER_FREE_ADDRESS_START;
  ps->mapped_info[HEAP_SEGMENT].npages = 0;
  ps->mapped_info[HEAP_SEGMENT].seg_type = HEAP_SEGMENT;
  //sprint("ps->user_heap.heap_bottom=%lx\n",ps->user_heap.heap_bottom);


  ps->total_mapped_region = 4;

  // return after initialization.
  return ps;
}

//
// reclaim a process. added @lab3_1
//
int free_process(process *proc) {
  // we set the status to ZOMBIE, but cannot destruct its vm space immediately.
  // since proc can be current process, and its user kernel stack is currently
  // in use! but for proxy kernel, it (memory leaking) may NOT be a really
  // serious issue, as it is different from regular OS, which needs to run 7x24.
  // proc->status = ZOMBIE;

  return 0;
}

//
// implements fork syscal in kernel. added @lab3_1
// basic idea here is to first allocate an empty process (child), then duplicate
// the context and data segments of parent process to the child, and lastly, map
// other segments (code, system) of the parent to child. the stack segment
// remains unchanged for the child.
//
int do_fork(process *parent) {
  int hartid = read_tp();
  sprint("will fork a child from parent %d.\n", parent->pid);
  process *child = alloc_process();
	if(parent->pid == 1){
		sprint("child pid 1 fork debug\n");
	}
  for (int i = 0; i < parent->total_mapped_region; i++) {
    // browse parent's vm space, and copy its trapframe and data segments,
    // map its code segment.
    switch (parent->mapped_info[i].seg_type) {
    case CONTEXT_SEGMENT:
      *child->trapframe = *parent->trapframe;
      break;
    case STACK_SEGMENT:
      memcpy((void *)lookup_pa(child->pagetable,
                               child->mapped_info[STACK_SEGMENT].va),
             (void *)lookup_pa(parent->pagetable, parent->mapped_info[i].va),
             PGSIZE);
      break;
    case HEAP_SEGMENT:
      for (uint64 heap_block = current[hartid]->user_heap.heap_bottom;
           heap_block < current[hartid]->user_heap.heap_top;
           heap_block += PGSIZE) {

        void *child_pa = Alloc_page();
        memcpy(child_pa, (void *)lookup_pa(parent->pagetable, heap_block),
               PGSIZE);
        user_vm_map((pagetable_t)child->pagetable, heap_block, PGSIZE,
                    (uint64)child_pa, prot_to_type(PROT_WRITE | PROT_READ, 1));
      }

      child->mapped_info[HEAP_SEGMENT].npages =
          parent->mapped_info[HEAP_SEGMENT].npages;

      memcpy((void *)&child->user_heap, (void *)&parent->user_heap,
             sizeof(parent->user_heap));
      break;
    case CODE_SEGMENT:
      // 在不考虑动态链接的情况下，ELF文件中只有一个代码段。
      uint64 num_pages = parent->mapped_info[i].npages;
      uint64 va_start = parent->mapped_info[i].va;

      for (uint64 i = 0; i < num_pages; i++) {
        map_pages(child->pagetable, va_start, PGSIZE,
                  lookup_pa(parent->pagetable, va_start + i * PGSIZE),
                  prot_to_type(PROT_EXEC | PROT_READ, 1));
      }

      // after mapping, register the vm region (do not delete codes below!)
      child->mapped_info[child->total_mapped_region].va =
          parent->mapped_info[i].va;
      child->mapped_info[child->total_mapped_region].npages =
          parent->mapped_info[i].npages;
      child->mapped_info[child->total_mapped_region].seg_type = CODE_SEGMENT;
      child->total_mapped_region++;
      break;
    }
  }

  child->status = READY;
  child->trapframe->regs.a0 = 0;
  child->parent = parent;
  insert_to_ready_queue(child);

  //sprint("do_fork ends\n");
  return child->pid;
}
