/*
 * Supervisor-mode startup codes
 */

#include "riscv.h"
#include "string.h"
#include "elf.h"
#include "process.h"
#include "pmm.h"
#include "vmm.h"
#include "memlayout.h"
#include "spike_interface/spike_utils.h"

// process is a structure defined in kernel/process.h
process user_app[2];

//
// trap_sec_start points to the beginning of S-mode trap segment (i.e., the entry point of
// S-mode trap vector). added @lab2_1
//
extern char trap_sec_start[];

//
// turn on paging. added @lab2_1
//
void enable_paging() {
  // write the pointer to kernel page (table) directory into the CSR of "satp".
  write_csr(satp, MAKE_SATP(g_kernel_pagetable));

  // refresh tlb to invalidate its content.
  flush_tlb();
}

void user_heap_init(process* proc){
  heap_block* heap_pa = (void*)alloc_page();
  //proc->heap = (void*)alloc_page();
  user_vm_map(proc->pagetable,USER_FREE_ADDRESS_START, PGSIZE,(uint64)heap_pa,prot_to_type(PROT_WRITE | PROT_READ, 1));
  proc->heap_size = PGSIZE;
  // 初始化堆内存块
  //heap_block* initial_block = proc->heap;
  heap_pa->size = proc->heap_size - sizeof(heap_block);  // 第一个块的大小是堆总大小减去元数据
  heap_pa->prev = NULL;
  heap_pa->next = NULL;
  heap_pa->free = 1;  // 初始块是空闲的
  proc->heap = (void*)USER_FREE_ADDRESS_START;
}
//
// load the elf, and construct a "process" (with only a trapframe).
// load_bincode_from_host_elf is defined in elf.c
//
void load_user_program(process *proc) {
  int hartid = read_tp();
  if(NCPU > 1)sprint("hartid = %d: ",hartid);
  sprint("User application is loading.\n");

  // 为进程控制块的各个成员指针分配物理内存
  proc->trapframe = (trapframe *)alloc_page();
  memset(proc->trapframe, 0, sizeof(trapframe));
  proc->pagetable = (pagetable_t)alloc_page();
  memset((void *)proc->pagetable, 0, PGSIZE);
  // 内核栈是自上而下增长的，所以说起始位置是页的高地址（左闭右开）
  proc->kstack = (uint64)alloc_page() + PGSIZE;
  uint64 user_stack_bottom = (uint64)alloc_page();

  // USER_STACK_TOP = 0x7ffff000, defined in kernel/memlayout.h
  proc->trapframe->regs.sp = USER_STACK_TOP;  //virtual address of user stack top
  proc->trapframe->regs.tp = hartid;


  user_heap_init(proc);
  sprint("user frame 0x%lx, user stack 0x%lx, user kstack 0x%lx \n", proc->trapframe,
         proc->trapframe->regs.sp, proc->kstack);

  load_bincode_from_host_elf(proc);

  // 为用户栈创建地址映射
  user_vm_map((pagetable_t)proc->pagetable, USER_STACK_TOP - PGSIZE, PGSIZE, user_stack_bottom,
              prot_to_type(PROT_WRITE | PROT_READ, 1));
  proc->user_stack_bottom = USER_STACK_TOP - PGSIZE;

  // 为中断上下文创建地址映射
  user_vm_map((pagetable_t)proc->pagetable, (uint64)proc->trapframe, PGSIZE, (uint64)proc->trapframe,
         prot_to_type(PROT_WRITE | PROT_READ, 0));

  // 因为用户模式触发中断时，使用的stvec=smode_trap_vector仍然是虚拟地址。
  // 所以说要把虚拟中断入口地址-->物理中断入口地址的虚实映射关系，也加入到用户模式的页表当中.
  // 这样才能让软中断成功跳转到正确的中断入口向量地址。
  user_vm_map((pagetable_t)proc->pagetable, (uint64)trap_sec_start, PGSIZE, (uint64)trap_sec_start,
         prot_to_type(PROT_READ | PROT_EXEC, 0));
  
}

//
// s_start: S-mode entry point of riscv-pke OS kernel.
//
volatile static int sig = 1;
int s_start(void) {
  int hartid = read_tp();
  if(NCPU > 1)sprint("hartid = %d: ",hartid);
  sprint("Enter supervisor mode...\n");
  // Note: we use direct (i.e., Bare mode) for memory mapping in lab1.
  // which means: Virtual Address = Physical Address
  // therefore, we need to set satp to be 0 for now. we will enable paging in lab2_x.
  // 
  // write_csr is a macro defined in kernel/riscv.h
  write_csr(satp, 0);

  if(hartid == 0){
    pmm_init();
    kern_vm_init();
    sig = 0;
  }
  while(sig){
    //sprint("hartid = %d: ",hartid);
    continue;
  }
  
  //sync_barrier(&sync_counter, NCPU);

  //  写入satp寄存器并刷新tlb缓存
  //    从这里开始，所有内存访问都通过MMU进行虚实转换
  enable_paging();

  if(NCPU > 1)sprint("hartid = %d: ",hartid);
  sprint("kernel page table is on \n");
  
  load_user_program(&(user_app[hartid]));

  if(NCPU > 1)sprint("hartid = %d: ",hartid);
  sprint("Switch to user mode...\n");
  // switch_to() is defined in kernel/process.c
  switch_to(&(user_app[hartid]));
  return 0;
}
