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
#include "sched.h"

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

  // reclaim the current process, and reschedule. added @lab3_1
  free_process( current );
  schedule();
  /*
  if(NCPU > 1)
    sync_barrier(&counter,NCPU);
  
  if (hartid == 0) {
    if (NCPU > 1)
      sprint("hartid = %d: ", hartid);
    sprint("shutdown with code:%d.\n", code);
    shutdown(code);
  }
  */

  return 0;
}

/*
uint64 sys_user_allocate_page() {
  void* pa = alloc_page();
  uint64 va;
  // if there are previously reclaimed pages, use them first (this does not change the
  // size of the heap)
  if (current->user_heap.free_pages_count > 0) {
    va =  current->user_heap.free_pages_address[--current->user_heap.free_pages_count];
    assert(va < current->user_heap.heap_top);
  } else {
    // otherwise, allocate a new page (this increases the size of the heap by one page)
    va = current->user_heap.heap_top;
    current->user_heap.heap_top += PGSIZE;

    current->mapped_info[HEAP_SEGMENT].npages++;
  }
  user_vm_map((pagetable_t)current->pagetable, va, PGSIZE, (uint64)pa,
         prot_to_type(PROT_WRITE | PROT_READ, 1));

  return va;
}
*/
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
// reclaim a page, indicated by "va". added @lab2_2
//
uint64 sys_user_free(uint64 va) {
  // user_vm_unmap((pagetable_t)current->pagetable, va, PGSIZE, 1);
  free((void *)va);
  return 0;
}
/*
uint64 sys_user_free_page(uint64 va) {
  user_vm_unmap((pagetable_t)current->pagetable, va, PGSIZE, 1);
  // add the reclaimed page to the free page list
  current->user_heap.free_pages_address[current->user_heap.free_pages_count++] = va;
  return 0;
}

*/
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
  case SYS_user_fork:
      return sys_user_fork();
  default:
    panic("Unknown syscall %ld \n", a0);
  }
}
