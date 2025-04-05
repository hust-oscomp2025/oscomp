#ifndef _TRAPFRAME_H_
#define _TRAPFRAME_H_

#include <kernel/riscv.h>
#include <kernel/types.h>

struct trapframe {
  // space to store context (all common registers)
  /* offset:0   */ riscv_regs regs;

  // process's "user kernel" stack
  /* offset:248 */ uint64 kernel_sp;
  // pointer to smode_trap_handler
  /* offset:256 */ uint64 kernel_trap;	// 这个字段目前弃用了，中断位置硬编码在汇编里
  // saved user process counter
  /* offset:264 */ uint64 epc;

  // kernel page table. added @lab2_1
  /* offset:272 */ uint64 kernel_satp;
	// kernel scheduler, added @lab3_challenge2
	/* offset:280 */ uint64 kernel_schedule;
};

#define store_all_registers(trapframe)                                                \
  asm volatile("sd ra, 0(%0)\n"                                                \
               "sd sp, 8(%0)\n"                                                \
               "sd gp, 16(%0)\n"                                               \
               "sd tp, 24(%0)\n"                                               \
               "sd t0, 32(%0)\n"                                               \
               "sd t1, 40(%0)\n"                                               \
               "sd t2, 48(%0)\n"                                               \
               "sd s0, 56(%0)\n"                                               \
               "sd s1, 64(%0)\n"                                               \
               "sd a0, 72(%0)\n"                                               \
               "sd a1, 80(%0)\n"                                               \
               "sd a2, 88(%0)\n"                                               \
               "sd a3, 96(%0)\n"                                               \
               "sd a4, 104(%0)\n"                                              \
               "sd a5, 112(%0)\n"                                              \
               "sd a6, 120(%0)\n"                                              \
               "sd a7, 128(%0)\n"                                              \
               "sd s2, 136(%0)\n"                                              \
               "sd s3, 144(%0)\n"                                              \
               "sd s4, 152(%0)\n"                                              \
               "sd s5, 160(%0)\n"                                              \
               "sd s6, 168(%0)\n"                                              \
               "sd s7, 176(%0)\n"                                              \
               "sd s8, 184(%0)\n"                                              \
               "sd s9, 192(%0)\n"                                              \
               "sd s10, 200(%0)\n"                                             \
               "sd s11, 208(%0)\n"                                             \
               "sd t3, 216(%0)\n"                                              \
               "sd t4, 224(%0)\n"                                              \
               "sd t5, 232(%0)\n"                                              \
               "sd t6, 240(%0)\n"                                              \
               :                                                               \
               : "r"(trapframe)                                                       \
               : "memory")
#define restore_all_registers(trapframe)                                              \
  asm volatile("ld ra, 0(%0)\n"                                                \
               "ld sp, 8(%0)\n"                                                \
               "ld gp, 16(%0)\n"                                               \
               "ld tp, 24(%0)\n"                                               \
               "ld t0, 32(%0)\n"                                               \
               "ld t1, 40(%0)\n"                                               \
               "ld t2, 48(%0)\n"                                               \
               "ld s0, 56(%0)\n"                                               \
               "ld s1, 64(%0)\n"                                               \
               "ld a0, 72(%0)\n"                                               \
               "ld a1, 80(%0)\n"                                               \
               "ld a2, 88(%0)\n"                                               \
               "ld a3, 96(%0)\n"                                               \
               "ld a4, 104(%0)\n"                                              \
               "ld a5, 112(%0)\n"                                              \
               "ld a6, 120(%0)\n"                                              \
               "ld a7, 128(%0)\n"                                              \
               "ld s2, 136(%0)\n"                                              \
               "ld s3, 144(%0)\n"                                              \
               "ld s4, 152(%0)\n"                                              \
               "ld s5, 160(%0)\n"                                              \
               "ld s6, 168(%0)\n"                                              \
               "ld s7, 176(%0)\n"                                              \
               "ld s8, 184(%0)\n"                                              \
               "ld s9, 192(%0)\n"                                              \
               "ld s10, 200(%0)\n"                                             \
               "ld s11, 208(%0)\n"                                             \
               "ld t3, 216(%0)\n"                                              \
               "ld t4, 224(%0)\n"                                              \
               "ld t5, 232(%0)\n"                                              \
               "ld t6, 240(%0)\n"                                              \
               :                                                               \
               : "r"(trapframe)                                                       \
               : "memory")

#endif