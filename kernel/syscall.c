/*
 * contains the implementation of all syscalls.
 */

#include <errno.h>
#include <stdint.h>

#include "elf.h"
#include "pmm.h"
#include "process.h"
#include "spike_interface/spike_utils.h"
#include "string.h"
#include "syscall.h"
#include "util/functions.h"
#include "util/types.h"
#include "vmm.h"
#include "sync_utils.h"

//
// implement the SYS_user_print syscall
//
ssize_t sys_user_print(const char *buf, size_t n) {
  int hartid = read_tp();
  // buf is now an address in user space of the given app's user stack,
  // so we have to transfer it into phisical address (kernel is running in
  // direct mapping).
  assert(current[hartid]);
  char *pa = (char *)user_va_to_pa((pagetable_t)(current[hartid]->pagetable),
                                   (void *)buf);
  if (NCPU > 1)
    sprint("hartid = %d: ", hartid);
  sprint("%s\n", pa);
  return 0;
}

//
// implement the SYS_user_exit syscall
//

volatile static int counter = 0;
ssize_t sys_user_exit(uint64 code) {
  int hartid = read_tp();
  if (NCPU > 1)
    sprint("hartid = %d: ", hartid);
  sprint("User exit with code:%d.\n", code);
  // in lab1, PKE considers only one app (one process).
  // therefore, shutdown the system when the app calls exit()
  if(NCPU > 1)
    sync_barrier(&counter,NCPU);
  
  if (hartid == 0) {
    if (NCPU > 1)
      sprint("hartid = %d: ", hartid);
    sprint("shutdown with code:%d.\n", code);
    shutdown(code);
  }

  return 0;
}

//
// maybe, the simplest implementation of malloc in the world ... added @lab2_2
//
uint64 sys_user_malloc(size_t size) {
  /*
    void *pa = alloc_page();
  uint64 va = g_ufree_page;
  g_ufree_page += PGSIZE;
  user_vm_map((pagetable_t)current->pagetable, va, PGSIZE, (uint64)pa,
         prot_to_type(PROT_WRITE | PROT_READ, 1));

  return va;
  */
  return (uint64)malloc(size);
}

//
// maybe, the simplest implementation of malloc in the world ... added @lab2_2
//
uint64 sys_user_allocate_page() {
  int hartid = read_tp();
  void* pa = alloc_page();
  uint64 va = g_ufree_page;
  g_ufree_page += PGSIZE;
  user_vm_map((pagetable_t)current[hartid]->pagetable, va, PGSIZE, (uint64)pa,
         prot_to_type(PROT_WRITE | PROT_READ, 1));
  sprint("hartid = ?: vaddr 0x%x is mapped to paddr 0x%x\n", va, pa);
  return va;
}


//
// reclaim a page, indicated by "va". added @lab2_2
//
uint64 sys_user_free(uint64 va) {
  // user_vm_unmap((pagetable_t)current->pagetable, va, PGSIZE, 1);
  free((void *)va);
  return 0;
}

// lab1_challenge1

ssize_t sys_user_print_backtrace(uint64 depth) {

  if (depth <= 0)
    return 0;
  int hartid = read_tp();
  trapframe *tf = current[hartid]->trapframe;
  uint64 temp_fp = tf->regs.s0;
  uint64 temp_pc = tf->epc;

  temp_fp = *(uint64 *)user_va_to_pa((pagetable_t)(current[hartid]->pagetable),
                                     (void *)temp_fp - 16);
  for (int i = 1; i <= depth; i++) {
    temp_pc = *(uint64 *)user_va_to_pa(
        (pagetable_t)(current[hartid]->pagetable), (void *)temp_fp - 8);
    char *function_name = locate_function_name(temp_pc);
    sprint("%s\n", function_name);
    if (strcmp(function_name, "main") == 0) {
      return i;
    } else {
      temp_fp = *(uint64 *)user_va_to_pa(
          (pagetable_t)(current[hartid]->pagetable), (void *)temp_fp - 16);
    }
  }
  return depth;
}

//
// [a0]: the syscall number; [a1] ... [a7]: arguments to the syscalls.
// returns the code of success, (e.g., 0 means success, fail for otherwise)
//
long do_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long a6,
                long a7) {
  switch (a0) {
  case SYS_user_print:
    return sys_user_print((const char *)a1, a2);
  case SYS_user_exit:
    return sys_user_exit(a1);
  // added @lab2_2
  case SYS_user_malloc:
    return sys_user_malloc(a1);
  case SYS_user_free:
    return sys_user_free(a1);
  case SYS_user_print_backtrace:
    return sys_user_print_backtrace(a1);
  default:
    panic("Unknown syscall %ld \n", a0);
  }
}
