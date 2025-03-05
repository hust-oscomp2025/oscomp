/*
 * Supervisor-mode startup codes
 */

#include "kernel/riscv.h"
#include <util/string.h>
#include <kernel/elf.h>
#include <kernel/process.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/sched.h>
#include <kernel/memlayout.h>
#include "spike_interface/spike_utils.h"
#include <kernel/types.h>
#include <kernel/vfs.h>
#include <kernel/rfs.h>
#include <kernel/ramdev.h>


//
// trap_sec_start points to the beginning of S-mode trap segment (i.e., the entry point of
// S-mode trap vector). added @lab2_1
//
// extern char trap_sec_start[];

//
// turn on paging. added @lab2_1
//
void enable_paging() {
  // write the pointer to kernel page (table) directory into the CSR of "satp".
  write_csr(satp, MAKE_SATP(g_kernel_pagetable));

  // refresh tlb to invalidate its content.
  flush_tlb();
  sprint("kernel page table is on \n");
}


typedef union {
  uint64 buf[MAX_CMDLINE_ARGS];
  char *argv[MAX_CMDLINE_ARGS];
} arg_buf;

//
// returns the number (should be 1) of string(s) after PKE kernel in command
// line. and store the string(s) in arg_bug_msg.
//
static size_t parse_args(arg_buf *arg_bug_msg) {
  // HTIFSYS_getmainvars frontend call reads command arguments to (input)
  // *arg_bug_msg
  long r = frontend_syscall(HTIFSYS_getmainvars, (uint64)arg_bug_msg,
                            sizeof(*arg_bug_msg), 0, 0, 0, 0, 0);
  kassert(r == 0);

  size_t pk_argc = arg_bug_msg->buf[0];
  uint64 *pk_argv = &arg_bug_msg->buf[1];

  int arg = 1; // skip the PKE OS kernel string, leave behind only the
               // application name
  for (size_t i = 0; arg + i < pk_argc; i++)
    arg_bug_msg->argv[i] = (char *)(uintptr_t)pk_argv[arg + i];

  // returns the number of strings after PKE kernel in command line
  return pk_argc - arg;
}



//
// load the elf, and construct a "process" (with only a trapframe).
// elf_load is defined in elf.c
//
process* load_user_program() {
  int hartid = read_tp();
  process* proc;
  proc = alloc_process();
	init_user_stack(proc);
	init_user_heap(proc);

	
  sprint("User application is loading.\n");
  arg_buf arg_bug_msg;
  // retrieve command line arguements
  size_t argc = parse_args(&arg_bug_msg);
  if (!argc)
    panic("You need to specify the application program!\n");
  load_elf_from_file(proc, arg_bug_msg.argv[hartid]);
  return proc;
}

//
// s_start: S-mode entry point of riscv-pke OS kernel.
//
volatile static int sig = 1;
int s_start(void) {

  sprint("Enter supervisor mode...\n");
  write_csr(satp, 0);

  int hartid = read_tp();
  if(hartid == 0){
    pmm_init();
    kern_vm_init();
    sig = 0;
  }
  while(sig){}
  
  //sync_barrier(&sync_counter, NCPU);

  //  写入satp寄存器并刷新tlb缓存
  //    从这里开始，所有内存访问都通过MMU进行虚实转换
  enable_paging();
  // added @lab3_1
  init_proc_pool();

  // init file system, added @lab4_1
  fs_init();

  sprint("Switch to user mode...\n");
  // the application code (elf) is first loaded into memory, and then put into execution
  // added @lab3_1
  insert_to_ready_queue( load_user_program() );
  schedule();

  // we should never reach here.
  return 0;
}
