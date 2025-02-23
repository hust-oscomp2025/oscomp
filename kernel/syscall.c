/*
 * contains the implementation of all syscalls.
 */

#include <errno.h>
#include <stdint.h>

#include "elf.h"
#include "global.h"
#include "pmm.h"
#include "process.h"
#include "sched.h"
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
  current[hartid]->status = ZOMBIE;
  sem_V(current[hartid]->sem_index);
  if (current[hartid]->parent != NULL)
    sem_V(current[hartid]->parent->sem_index);

  // reclaim the current process, and reschedule. added @lab3_1
  // 这个过程只在父进程显式调用wait时执行。如果说父进程提前退出，那么子进程应该被init进程收养。
  // free_process(current[hartid]);
  schedule();

  return 0;
}

uint64 sys_user_malloc(size_t size) { return (uint64)vmalloc(size); }

//
// reclaim a page, indicated by "va". added @lab2_2
//
uint64 sys_user_free(uint64 va) {
  // user_vm_unmap((pagetable_t)current->pagetable, va, PGSIZE, 1);
  free((void *)va);
  return 0;
}

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

ssize_t sys_user_fork() {
  int hartid = read_tp();
  sprint("User call fork.\n");
  return do_fork(current[hartid]);
}

volatile int sys_user_wait(int pid) {
  // sprint("DEBUG LINE, pid = %d\n",pid);

  int hartid = read_tp();
  // int child_found_flag = 0;
  if (pid == -1) {
    while (1) {
      for (int i = 0; i < NPROC; i++) {
        // sprint("DEBUG LINE\n");

        process *p = &(procs[i]);
        // sprint("p = 0x%lx,\n",p);
        if (p->parent != NULL && p->parent->pid == current[hartid]->pid &&
            p->status == ZOMBIE) {
          // sprint("DEBUG LINE\n");

          free_process(p);
          return i;
        }
      }
      // sprint("current[hartid]->sem_index = %d\n",current[hartid]->sem_index);
      sem_P(current[hartid]->sem_index);
      //sprint("wait:return from blocking!\n");
    }
  }
  if (0 < pid && pid < NPROC) {
    // sprint("DEBUG LINE\n");

    process *p = &procs[pid];
    if (p->parent != current[hartid]) {
      return -1;
    } else if (p->status == ZOMBIE) {
      free_process(p);
      return pid;
    } else {
      sem_P(p->sem_index);
      sprint("return from blocking!\n");

      return pid;
    }
  }
  return -1;
}

int sys_user_sem_new(int initial_value) {
  //int pid = current[read_tp()]->pid;
  return sem_new(initial_value);
}

int sys_user_sem_P(int sem_index) {
  int hartid = read_tp();
  return sem_P(sem_index);
}

int sys_user_sem_V(int sem_index) {
  int hartid = read_tp();
  return sem_V(sem_index);
}

ssize_t sys_user_yield() {
  // TODO (lab3_2): implment the syscall of yield.
  // hint: the functionality of yield is to give up the processor. therefore,
  // we should set the status of currently running process to READY, insert it
  // in the rear of ready queue, and finally, schedule a READY process to run.
  // panic( "You need to implement the yield syscall in lab3_2.\n" );
  int hartid = read_tp();
  //current[hartid]->status = READY;
  insert_to_ready_queue(current[hartid]);
  schedule();
  return 0;
}

int sys_user_test() {
  // 第1步：通过 kmalloc 分配 100 字节的内存
  char *test_mem = (char *)kmalloc(100); // 假设每次分配 100 字节
  sprint("test_mem=0x%x\n", test_mem);

  // 检查分配是否成功
  if (test_mem == NULL) {
    sprint("kmalloc failed to allocate memory\n");
    return -1;
  }

  // 第2步：进行简单的写入操作（使用 kmalloc 分配的内存）
  for (int i = 0; i < 100; i++) {
    test_mem[i] = 'A' + (i % 26); // 填充字符 'A' 到 'Z'，然后循环
  }

  // 第3步：进行读取操作，验证写入是否正确
  for (int i = 0; i < 100; i++) {
    if (test_mem[i] != 'A' + (i % 26)) {
      sprint("Test failed at index %d. Expected '%c' but got '%c'.\n", i,
             'A' + (i % 26), test_mem[i]);
      kfree(test_mem); // 释放内存
      return -1;
    }
  }

  sprint("kmalloc and memory write test passed!\n");

  // 第4步：通过 kfree 释放内存
  kfree(test_mem);

  // 第5步：再次分配新的内存块，验证内存释放后能否正确使用
  test_mem = (char *)kmalloc(50); // 试着分配 50 字节
  sprint("test_mem=0x%x\n", test_mem);
  test_mem = (char *)kmalloc(3950); // 试着分配 50 字节
  sprint("test_mem=0x%x\n", test_mem);
  test_mem = (char *)kmalloc(50); // 试着分配 50 字节
  sprint("test_mem=0x%x\n", test_mem);
  test_mem = (char *)kmalloc(4000); // 试着分配 50 字节
  sprint("test_mem=0x%x\n", test_mem);
  if (test_mem == NULL) {
    sprint("kmalloc failed to allocate new memory after kfree\n");
    return -1;
  }

  // 填充新分配的内存并验证
  for (int i = 0; i < 50; i++) {
    test_mem[i] = 'a' + (i % 26); // 填充字符 'a' 到 'z'，然后循环
  }

  // 验证是否写入正确
  for (int i = 0; i < 50; i++) {
    if (test_mem[i] != 'a' + (i % 26)) {
      sprint("Test failed at index %d. Expected '%c' but got '%c'.\n", i,
             'a' + (i % 26), test_mem[i]);
      kfree(test_mem);
      return -1;
    }
  }

  sprint("kmalloc and kfree memory test passed again!\n");

  // 释放新分配的内存
  kfree(test_mem);
  return 0;
}

ssize_t sys_user_printpa(uint64 va)
{
  uint64 pa = (uint64)user_va_to_pa((pagetable_t)(current->pagetable), (void*)va);
  sprint("%lx\n", pa);
  return 0;
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
  case SYS_user_fork:
    // int ret = sys_user_fork();
    // sprint("DEBUG LINE\n");
    return sys_user_fork();
  case SYS_user_yield:
    return sys_user_yield();
    case SYS_user_printpa:
      return sys_user_printpa(a1);
  case SYS_user_wait:
    //int ret = sys_user_wait(a1);
    //sprint("do_syscall:return from blocking!\n");

    return sys_user_wait(a1);
  case SYS_user_test:
    return sys_user_test();
  case SYS_user_sem_new:
    return sys_user_sem_new(a1);
  case SYS_user_sem_P:
    return sys_user_sem_P(a1);
  case SYS_user_sem_V:
    return sys_user_sem_V(a1);
  default:
    panic("Unknown syscall %ld \n", a0);
  }
}
