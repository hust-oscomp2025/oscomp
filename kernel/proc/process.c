/*
 * Utility functions for process management.
 *
 * Note: in Lab1, only one process (i.e., our user application) exists.
 * Therefore, PKE OS at this stage will set "current" to the loaded user
 * application, and also switch to the old "current" process after trap
 * handling.
 */

#include <kernel/config.h>
#include <kernel/elf.h>
#include <kernel/memlayout.h>
#include <kernel/mm_struct.h>
#include <kernel/mmap.h>

#include <kernel/process.h>
#include <kernel/riscv.h>
#include <kernel/sched.h>
#include <kernel/semaphore.h>
#include <kernel/strap.h>

#include <spike_interface/spike_utils.h>
#include <util/string.h>


process procs[NPROC];
process *current_percpu[NCPU];
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
  return_to_user(proc->trapframe, MAKE_SATP(proc->mm->pagetable));
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

process *find_empty_process() {
  for (int i = 0; i < NPROC; i++) {
    if (procs[i].status == FREE) {
      return &(procs[i]);
    }
  }
  panic("cannot find any free process structure.\n");
}



process *alloc_process() {
  // locate the first usable process structure
  process *ps = find_empty_process();
  ps->mm = user_mm_create();
  // 分配内核栈
  ps->kstack = (uint64)alloc_page()->virtual_address + PGSIZE;

  ps->trapframe = (trapframe *)alloc_page()->virtual_address;

  // 创建信号量和初始化文件管理
  ps->sem_index = sem_new(0);
  ps->ktrapframe = NULL;
  ps->pfiles = init_proc_file_management();

  sprint("in alloc_proc. build proc_file_management successfully.\n");
  return ps;
}

int free_process(process *proc) {
  // 在exit中把进程的状态设成ZOMBIE，然后在父进程wait中调用这个函数，用于释放子进程的资源（待实现）
  // 由于代理内核的特殊机制，不做也不会造成内存泄漏（代填）

  return 0;
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
        if (p->parent != NULL && p->parent->pid == CURRENT->pid &&
            p->status == ZOMBIE) {
          // sprint("DEBUG LINE\n");

          free_process(p);
          return i;
        }
      }
      // sprint("current->sem_index = %d\n",current->sem_index);
      sem_P(CURRENT->sem_index);
      // sprint("wait:return from blocking!\n");
    }
  }
  if (0 < pid && pid < NPROC) {
    // sprint("DEBUG LINE\n");

    process *p = &procs[pid];
    if (p->parent != CURRENT) {
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
