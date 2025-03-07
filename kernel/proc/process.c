/*
 * Utility functions for process management.
 *
 * Note: in Lab1, only one process (i.e., our user application) exists.
 * Therefore, PKE OS at this stage will set "current" to the loaded user
 * application, and also switch to the old "current" process after trap
 * handling.
 */

#include <kernel/process.h>
#include <kernel/config.h>
#include <kernel/elf.h>

#include <kernel/memlayout.h>
#include <kernel/pmm.h>
#include <kernel/riscv.h>
#include <kernel/sched.h>
#include "spike_interface/spike_utils.h"
#include <kernel/strap.h>
#include <util/string.h>
#include <kernel/vmm.h>
#include <kernel/semaphore.h>

// moved to global.h
//
// extern char smode_trap_vector[];
// extern void return_to_user(trapframe *, uint64 satp);
// extern char trap_sec_start[];

// moved to global.c
// process procs[NPROC];
// process* current[NCPU];

process procs[NPROC];
process* current_percpu[NCPU];
//
// switch to a user-mode process
//

extern void return_to_user(trapframe *, uint64 satp);



void switch_to(process *proc) {

  assert(proc);
  CURRENT = proc;
	
	extern char smode_trap_vector[];
  write_csr(stvec, (uint64)smode_trap_vector);
  // set up trapframe values (in process structure) that smode_trap_vector will
  // need when the process next re-enters the kernel.
  proc->trapframe->kernel_sp = proc->kstack;     // process's kernel stack
  proc->trapframe->kernel_satp = read_csr(satp); // kernel page table
  proc->trapframe->kernel_trap = (uint64)smode_trap_handler;
  proc->trapframe->kernel_schedule = (uint64)schedule;

  // SSTATUS_SPP and SSTATUS_SPIE are defined in kernel/riscv.h
  // set S Previous Privilege mode (the SSTATUS_SPP bit in sstatus register) to
  // User mode,to to enable interrupts, and sret destination.

  write_csr(sstatus, ((read_csr(sstatus) & ~SSTATUS_SPP) | SSTATUS_SPIE));

  // set S Exception Program Counter (sepc register) to the elf entry pc.
  write_csr(sepc, proc->trapframe->epc);
	sprint("return to user\n");
  return_to_user(proc->trapframe, MAKE_SATP(proc->pagetable));
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

void init_user_stack(process *ps) {
  ps->trapframe->regs.sp = USER_STACK_TOP - 8; // virtual address of user stack top
  uint64 user_stack =
      (uint64)Alloc_page(); // phisical address of user stack bottom
  // map user stack in userspace
  user_vm_map((pagetable_t)ps->pagetable, USER_STACK_TOP - PGSIZE, PGSIZE,
              user_stack, prot_to_type(PROT_WRITE | PROT_READ, 1));
  ps->mapped_info[STACK_SEGMENT].va = USER_STACK_TOP - PGSIZE;
  ps->mapped_info[STACK_SEGMENT].npages = 1;
  ps->mapped_info[STACK_SEGMENT].seg_type = STACK_SEGMENT;
  ps->total_mapped_region++;
}

void init_user_heap(process *ps) {
  // initialize the process's heap manager
  ps->user_heap.heap_top = USER_FREE_ADDRESS_START;
  ps->user_heap.heap_bottom = USER_FREE_ADDRESS_START;
  // ps->user_heap.free_pages_address = USER_FREE_ADDRESS_START;
  // ps->user_heap.free_pages_count = 0;

  // map user heap in userspace
  ps->mapped_info[HEAP_SEGMENT].va = USER_FREE_ADDRESS_START;
  ps->mapped_info[HEAP_SEGMENT].npages = 0;
  ps->mapped_info[HEAP_SEGMENT].seg_type = HEAP_SEGMENT;
  // sprint("ps->user_heap.heap_bottom=%lx\n",ps->user_heap.heap_bottom);
  ps->total_mapped_region++;
}

//
// allocate an empty process, init its vm space. returns the pointer to
// process strcuture. added @lab3_1
//

process *find_empty_process() {
  for (int i = 0; i < NPROC; i++) {
    if (procs[i].status == FREE) {
      return &(procs[i]);
    }
  }
  panic("cannot find any free process structure.\n");
}



extern char trap_sec_start[];
process *alloc_process() {
  // locate the first usable process structure
  process *ps = find_empty_process();

  // 首先为进程页表，和映射表分配空间
  ps->pagetable = (pagetable_t)Alloc_page();
  ps->mapped_info = (mapped_region *)Alloc_page();

  // 分配内核栈
  ps->kstack = (uint64)Alloc_page() + PGSIZE;

  // 为进程中断上下文分配空间并记录
  ps->trapframe = (trapframe *)Alloc_page();
  user_vm_map((pagetable_t)ps->pagetable, (uint64)ps->trapframe, PGSIZE,
              (uint64)ps->trapframe, prot_to_type(PROT_WRITE | PROT_READ, 0));
  ps->mapped_info[CONTEXT_SEGMENT].va = (uint64)ps->trapframe;
  ps->mapped_info[CONTEXT_SEGMENT].npages = 1;
  ps->mapped_info[CONTEXT_SEGMENT].seg_type = CONTEXT_SEGMENT;
  ps->total_mapped_region++;

  // 为进程中断入口程序映射空间并记录
  user_vm_map((pagetable_t)ps->pagetable, (uint64)trap_sec_start, PGSIZE,
              (uint64)trap_sec_start, prot_to_type(PROT_READ | PROT_EXEC, 0));
  ps->mapped_info[SYSTEM_SEGMENT].va = (uint64)trap_sec_start;
  ps->mapped_info[SYSTEM_SEGMENT].npages = 1;
  ps->mapped_info[SYSTEM_SEGMENT].seg_type = SYSTEM_SEGMENT;
  ps->total_mapped_region++;

  // 创建进程信号量，在wait(pid)系统调用中使用。
  ps->sem_index = sem_new(0);
  ps->ktrapframe = NULL;
  // initialize files_struct
  ps->pfiles = init_proc_file_management();
  sprint("in alloc_proc. build proc_file_management successfully.\n");
  return ps;
}

int free_process(process *proc) {
  // 在exit中把进程的状态设成ZOMBIE，然后在父进程wait中调用这个函数，用于释放子进程的资源（待实现）
  // 由于代理内核的特殊机制，不做也不会造成内存泄漏（代填）

  return 0;
}

void fork_segment(process *parent, process *child, int segnum, int choice) {
  mapped_region *mapped_info = &parent->mapped_info[segnum];
  for (int i = 0; i < mapped_info->npages; i++) {
    uint64 pa = lookup_pa(parent->pagetable, mapped_info->va + i * PGSIZE);
    if (choice == FORK_COPY) {
      uint64 newpa = (uint64)Alloc_page();
      memcpy((void *)newpa, (void *)pa, PGSIZE);
      user_vm_map((pagetable_t)child->pagetable, mapped_info->va + i * PGSIZE,
                  PGSIZE, newpa, prot_to_type(PROT_WRITE | PROT_READ, 1));
    } else if (choice == FORK_COW) {
      user_vm_map((pagetable_t)child->pagetable, mapped_info->va + i * PGSIZE,
                  PGSIZE, pa, prot_to_type(PROT_READ, 1));
      user_vm_unmap((pagetable_t)parent->pagetable,
                    mapped_info->va + i * PGSIZE, PGSIZE, 0);
      // sprint("debug");
      user_vm_map((pagetable_t)parent->pagetable, mapped_info->va + i * PGSIZE,
                  PGSIZE, pa, prot_to_type(PROT_READ, 1));
    } else {
      user_vm_map((pagetable_t)child->pagetable, mapped_info->va + i * PGSIZE,
                  PGSIZE, pa, prot_to_type(PROT_EXEC | PROT_READ, 1));
    }
  }
  memcpy(&(child->mapped_info[segnum]), mapped_info, sizeof(mapped_region));
  child->total_mapped_region++;
}

int do_fork(process *parent) {
  int hartid = read_tp();
  sprint("will fork a child from parent %d.\n", parent->pid);
  process *child = alloc_process();
  for (int i = 0; i < parent->total_mapped_region; i++) {
    // browse parent's vm space, and copy its trapframe and data segments,
    // map its code segment.
    switch (parent->mapped_info[i].seg_type) {
    case CONTEXT_SEGMENT:
      *child->trapframe = *parent->trapframe;
      break;
    case STACK_SEGMENT:
      fork_segment(parent, child, i, FORK_COW);
      break;
    case HEAP_SEGMENT:
      fork_segment(parent, child, i, FORK_COW);
      break;
    case CODE_SEGMENT:
      // 代码段不需要复制
      fork_segment(parent, child, i, FORK_MAP);
      break;
    case DATA_SEGMENT:
      fork_segment(parent, child, i, FORK_COW);
      break;
    }
  }

  // child->status = READY;
  child->trapframe->regs.a0 = 0;
  child->parent = parent;
  insert_to_ready_queue(child);

  // sprint("do_fork ends\n");
  return child->pid;
}

static void unmap_segment(process *ps, int segnum) {
  mapped_region *mapped_info = &ps->mapped_info[segnum];
  for (int i = 0; i < mapped_info->npages; i++) {
    user_vm_unmap((pagetable_t)ps->pagetable, mapped_info->va + i * PGSIZE,
                  PGSIZE, 1);
  }
  memset(mapped_info, 0, sizeof(mapped_region));
  ps->total_mapped_region--;
}


/**
 * @brief 执行可执行的ELF文件，或者shell脚本
 */
int do_exec(void *path) {
  //, char **argv, u64 envp
  // 当前只支持进程中仅有一个线程时进行 exec
  process *cur = CURRENT;

  for (int i = 0; i < cur->total_mapped_region; i++) {
    switch (cur->mapped_info[i].seg_type) {
    case STACK_SEGMENT:
      unmap_segment(cur, i);
      break;
    case CONTEXT_SEGMENT:
      break;
    case SYSTEM_SEGMENT:
      break;
    case HEAP_SEGMENT:
      cur->user_heap.heap_top = USER_FREE_ADDRESS_START;
      cur->user_heap.heap_bottom = USER_FREE_ADDRESS_START;
      unmap_segment(cur, i);
      break;
    case CODE_SEGMENT:
			unmap_segment(cur, i);
			break;
    case DATA_SEGMENT:
			unmap_segment(cur, i);
      break;
    default:
      panic("unknown segment type encountered, segment type:%d.\n",
            cur->mapped_info[i].seg_type);
    }
  }
	init_user_stack(cur);
	init_user_heap(cur);
	load_elf_from_file(cur,path);
	insert_to_ready_queue(cur);
	schedule();



	sprint("exec failed.\n");
	return -1;
}


ssize_t do_wait(int pid) {
  // sprint("DEBUG LINE, pid = %d\n",pid);
	extern process procs[NPROC];
  int hartid = read_tp();
  // int child_found_flag = 0;
  if (pid == -1) {
    while (1) {
      for (int i = 0; i < NPROC; i++) {
        // sprint("DEBUG LINE\n");

        process *p = &(procs[i]);
        // sprint("p = 0x%lx,\n",p);
        if (p->parent != NULL && p->parent->pid == current_percpu[hartid]->pid &&
            p->status == ZOMBIE) {
          // sprint("DEBUG LINE\n");

          free_process(p);
          return i;
        }
      }
      // sprint("current->sem_index = %d\n",current->sem_index);
      sem_P(current_percpu[hartid]->sem_index);
      // sprint("wait:return from blocking!\n");
    }
  }
  if (0 < pid && pid < NPROC) {
    // sprint("DEBUG LINE\n");

    process *p = &procs[pid];
    if (p->parent != current_percpu[hartid]) {
      return -1;
    } else if (p->status == ZOMBIE) {
      free_process(p);
      return pid;
    } else {
      sem_P(p->sem_index);
      // sprint("return from blocking!\n");

      return pid;
    }
  }
  return -1;
}
