/*
 * Supervisor-mode startup codes
 */

#include <kernel/riscv.h>
#include <kernel/elf.h>
#include <kernel/process.h>

#include <kernel/sched.h>
#include <kernel/mm/memlayout.h>
#include <kernel/types.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/rfs.h>
#include <kernel/fs/ramdev.h>
#include <kernel/mm/pagetable.h>
#include <kernel/mm/kmalloc.h>

#include <util/string.h>

#include <spike_interface/spike_utils.h>


extern char _end[];
extern uint64 g_mem_size;
static uint64 free_mem_start_addr;  //beginning address of free memory
static uint64 free_mem_end_addr;    //end address of free memory (not included)
static void pmm_init() {
  // 内核程序段起止地址
  uint64 g_kernel_start = KERN_BASE;
  uint64 g_kernel_end = (uint64)&_end;

  uint64 pke_kernel_size = g_kernel_end - g_kernel_start;
  sprint("PKE kernel start 0x%lx, PKE kernel end: 0x%lx, PKE kernel size: 0x%lx.\n",
    g_kernel_start, g_kernel_end, pke_kernel_size);

  // 空闲内存起始地址必须页对齐
  free_mem_start_addr = ROUNDUP(g_kernel_end, PGSIZE);


  // 重新计算g_mem_size以限制物理内存空间
  g_mem_size = ROUNDDOWN((PKE_MAX_ALLOWABLE_RAM, g_mem_size),PGSIZE);
	assert(g_mem_size > pke_kernel_size);
  sprint("free physical memory address: [0x%lx, 0x%lx] \n", free_mem_start_addr,
    DRAM_BASE + g_mem_size - 1);
  


  // 初始化页管理子系统
  page_init(DRAM_BASE, g_mem_size, free_mem_start_addr);
  

  sprint("Physical memory manager initialization complete.\n");
}

extern char _etext[];
static void kern_vm_init(void) {
  g_kernel_pagetable = alloc_page()->virtual_address;
  // 首先分配一个页当内核的页表

  // map virtual address [KERN_BASE, _etext] to physical address [DRAM_BASE,
  // DRAM_BASE+(_etext - KERN_BASE)], to maintin (direct) text section kernel
  // address mapping.
  for (int i = KERN_BASE; i <= ROUNDDOWN((uint64)_etext, PAGE_SIZE);
       i += PAGE_SIZE) {
    pgt_map_page(g_kernel_pagetable, i, i, PTE_R | PTE_X);
  }
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
	pagetable_activate(g_kernel_pagetable);
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
