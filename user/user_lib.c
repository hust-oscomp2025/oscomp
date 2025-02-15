/*
 * The supporting library for applications.
 * Actually, supporting routines for applications are catalogued as the user 
 * library. we don't do that in PKE to make the relationship between application 
 * and user library more straightforward.
 */

#include "user_lib.h"
#include "util/types.h"
#include "util/snprintf.h"
#include "kernel/syscall.h"



int do_user_call(uint64 sysnum, uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5, uint64 a6,
                 uint64 a7) {
  int ret;

  // before invoking the syscall, arguments of do_user_call are already loaded into the argument
  // registers (a0-a7) of our (emulated) risc-v machine.
  asm volatile(
      "ecall\n"
      "sw a0, %0"  // returns a 32-bit value
      : "=m"(ret)
      :
      : "memory");

  return ret;
}

//
// printu() supports user/lab1_1_helloworld.c
//
int printu(const char* s, ...) {
  va_list vl;
  va_start(vl, s);

  char out[256];  // fixed buffer size.
  int res = vsnprintf(out, sizeof(out), s, vl);
  va_end(vl);


  const char* buf = out;
  size_t n = res < sizeof(out) ? res : sizeof(out);

  // make a syscall to implement the required functionality.
  return do_user_call(SYS_user_print, (uint64)buf, n, 0, 0, 0, 0, 0);
}

void printRegs(){
  printu("========printing reg status========\n");
  uint64 reg_value;
    // 手动读取并打印每个寄存器的值
    __asm__ volatile("mv %0, x0" : "=r"(reg_value));
    printu("x0  = 0x%lx\n", reg_value);
    
    __asm__ volatile("mv %0, x1" : "=r"(reg_value));
    printu("x1  = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x2" : "=r"(reg_value));
    printu("x2  = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x3" : "=r"(reg_value));
    printu("x3  = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x4" : "=r"(reg_value));
    printu("x4  = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x5" : "=r"(reg_value));
    printu("x5  = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x6" : "=r"(reg_value));
    printu("x6  = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x7" : "=r"(reg_value));
    printu("x7  = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x8" : "=r"(reg_value));
    printu("x8  = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x9" : "=r"(reg_value));
    printu("x9  = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x10" : "=r"(reg_value));
    printu("x10 = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x11" : "=r"(reg_value));
    printu("x11 = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x12" : "=r"(reg_value));
    printu("x12 = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x13" : "=r"(reg_value));
    printu("x13 = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x14" : "=r"(reg_value));
    printu("x14 = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x15" : "=r"(reg_value));
    printu("x15 = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x16" : "=r"(reg_value));
    printu("x16 = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x17" : "=r"(reg_value));
    printu("x17 = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x18" : "=r"(reg_value));
    printu("x18 = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x19" : "=r"(reg_value));
    printu("x19 = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x20" : "=r"(reg_value));
    printu("x20 = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x21" : "=r"(reg_value));
    printu("x21 = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x22" : "=r"(reg_value));
    printu("x22 = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x23" : "=r"(reg_value));
    printu("x23 = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x24" : "=r"(reg_value));
    printu("x24 = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x25" : "=r"(reg_value));
    printu("x25 = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x26" : "=r"(reg_value));
    printu("x26 = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x27" : "=r"(reg_value));
    printu("x27 = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x28" : "=r"(reg_value));
    printu("x28 = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x29" : "=r"(reg_value));
    printu("x29 = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x30" : "=r"(reg_value));
    printu("x30 = 0x%lx\n", reg_value);

    __asm__ volatile("mv %0, x31" : "=r"(reg_value));
    printu("x31 = 0x%lx\n", reg_value);

    // 打印栈指针（SP）、栈帧指针（FP）和程序计数器（PC）
    uint64 sp, fp, pc;
    __asm__ volatile (
        "mv %0, sp" : "=r"(sp)
    );
    __asm__ volatile (
        "mv %0, fp" : "=r"(fp)
    );
    //__asm__ volatile (
    //    "mv %0, ra" : "=r"(pc) // ra 寄存器作为程序计数器
    //);

    printu("SP = 0x%lx\n", sp);
    printu("FP = 0x%lx\n", fp);
    printu("PC = 0x%lx\n", getRa());
}

// 获取调用 print_registers 的程序计数器（PC）
int getRa(void) {
    uint64 return_address;
    
    // 使用 fp 获取返回地址
    __asm__ volatile (
        "mv %0, fp"           // 将 fp 寄存器的值（栈帧指针）保存到 %0
        : "=r"(return_address) // 输出参数
    );

    // 返回栈帧中的返回地址
    return return_address - 4;  // 获取栈帧中的返回地址
}



//
// applications need to call exit to quit execution.
//
int exit(int code) {
  return do_user_call(SYS_user_exit, code, 0, 0, 0, 0, 0, 0); 
}
