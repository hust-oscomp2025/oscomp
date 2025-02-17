/*
 * contains the implementation of all syscalls.
 */

#include <stdint.h>
#include <errno.h>

#include "util/types.h"
#include "syscall.h"
#include "string.h"
#include "process.h"
#include "util/functions.h"
#include "elf.h"

#include "spike_interface/spike_utils.h"

//
// implement the SYS_user_print syscall
//
ssize_t sys_user_print(const char* buf, size_t n) {
  sprint("hartid = ?: %s\n", buf);
  return 0;
}

//
// implement the SYS_user_exit syscall
//
ssize_t sys_user_exit(uint64 code) {
  sprint("hartid = ?: User exit with code:%d.\n", code);
  // in lab1, PKE considers only one app (one process). 
  // therefore, shutdown the system when the app calls exit()
  sprint("hartid = ?: shutdown with code:%d.\n", code);
  shutdown(code);
}

//lab1_challenge1
ssize_t sys_user_print_backtrace(uint64 depth) {
  if(depth <= 0) return 0;
  trapframe* tf = current->trapframe;
  void* temp_fp = (void*)(tf->regs.s0);
  
  uint64 temp_pc = tf->epc;
  // 跳过do_user_call的栈帧
  temp_fp = (void*)(*((uint64*)(temp_fp - 16)));
  // 然后栈帧位于print_backtrace，我们不使用中断epc去找它的函数名，直接使用ra去找上一级调用print_backtrace的调用点
  // sprint("temp_fp=%lx\n",temp_fp);
  // sprint("temp_pc=%lx\n",temp_pc);
  // sprint("%s\n",locate_function_name(temp_pc));
  for(int i = 1;i <= depth;i++){
    temp_pc = *((uint64*)(temp_fp-8));
    // sprint("temp_fp=%lx\n",temp_fp);
    // sprint("temp_pc=%lx\n",temp_pc);
    char* function_name = locate_function_name(temp_pc);
    sprint("%s\n",function_name);
    if(strcmp(function_name,"main") == 0){
      return i;
    }else{
      temp_fp = (void*)(*((uint64*)(temp_fp - 16)));
      //sprint("temp_fp=%lx\n",temp_fp);
    }
  }
  return depth;
}

//
// [a0]: the syscall number; [a1] ... [a7]: arguments to the syscalls.
// returns the code of success, (e.g., 0 means success, fail for otherwise)
//
long do_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
  switch (a0) {
    case SYS_user_print:
      return sys_user_print((const char*)a1, a2);
    case SYS_user_exit:
      return sys_user_exit(a1);
    case SYS_user_print_backtrace:
      return sys_user_print_backtrace(a1);
    default:
      panic("Unknown syscall %ld \n", a0);
  }
}
