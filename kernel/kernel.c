/*
 * Supervisor-mode startup codes
 */

#include <kernel/riscv.h>
#include <kernel/elf.h>
#include <kernel/process.h>

#include <kernel/sched.h>

#include <kernel/types.h>
#include <kernel/fs/vfs.h>


#include <kernel/mm/pagetable.h>
#include <kernel/mm/slab.h>

#include <util/string.h>
#include <util/spinlock.h>

#include <spike_interface/spike_utils.h>





extern char _etext[];
extern char _edata[];
static void kern_vm_init(void) {
	// // 分配一个页作为内核的根页表
	// g_kernel_pagetable = alloc_page()->virtual_address;
	// memset(g_kernel_pagetable, 0, PAGE_SIZE);
	// sprint("kern_vm_init: global pagetable page address: %lx\n", g_kernel_pagetable);
	// sprint("kern_vm_init: pagetable memory address: %lx\n", &g_kernel_pagetable);
	
	// // 获取页表的物理地址
	// uint64 pagetable_pa = VIRTUAL_TO_PHYSICAL(g_kernel_pagetable);
	
	// // 1. 首先映射页表自身，确保在启用 MMU 后还能访问页表
	// pgt_map_page(g_kernel_pagetable, (uint64)g_kernel_pagetable, pagetable_pa, PTE_R | PTE_W);
	
	// // 2. 映射内核代码段 (.text)
	// uint64 text_start = KERN_BASE;
	// uint64 text_end = ROUNDUP((uint64)_etext, PAGE_SIZE);
	// sprint("kern_vm_init: mapping text section [%lx-%lx]\n", text_start, text_end);
	
	// for (uint64 va = text_start, pa = VIRTUAL_TO_PHYSICAL(va); 
	// 		 va < text_end; 
	// 		 va += PAGE_SIZE, pa += PAGE_SIZE) {
	// 		pgt_map_page(g_kernel_pagetable, va, pa, PTE_R | PTE_X); // 代码段：可读可执行
	// }
	
	// // 3. 映射内核数据段 (.data)
	// uint64 data_start = ROUNDDOWN((uint64)_etext, PAGE_SIZE);
	// uint64 data_end = ROUNDUP((uint64)_edata, PAGE_SIZE);
	// sprint("kern_vm_init: mapping data section [%lx-%lx]\n", data_start, data_end);
	
	// for (uint64 va = data_start, pa = VIRTUAL_TO_PHYSICAL(va); 
	// 		 va < data_end; 
	// 		 va += PAGE_SIZE, pa += PAGE_SIZE) {
	// 		pgt_map_page(g_kernel_pagetable, va, pa, PTE_R | PTE_W); // 数据段：可读可写
	// }
	
	// // 4. 映射内核 BSS 段
	// uint64 bss_start = ROUNDDOWN((uint64)_edata, PAGE_SIZE);
	// uint64 bss_end = ROUNDUP((uint64)_end, PAGE_SIZE);
	// sprint("kern_vm_init: mapping bss section [%lx-%lx]\n", bss_start, bss_end);
	
	// for (uint64 va = bss_start, pa = VIRTUAL_TO_PHYSICAL(va); 
	// 		 va < bss_end; 
	// 		 va += PAGE_SIZE, pa += PAGE_SIZE) {
	// 		pgt_map_page(g_kernel_pagetable, va, pa, PTE_R | PTE_W); // BSS段：可读可写
	// }
	//g_kernel_pagetable = alloc_page()->virtual_address;
	g_kernel_pagetable = alloc_page()->virtual_address;
	memset(g_kernel_pagetable, 0, PAGE_SIZE);
	for (uint64 va = KERN_BASE, pa = KERN_BASE; 
			 va < DRAM_BASE + PKE_MAX_ALLOWABLE_RAM; 
			 va += PAGE_SIZE, pa += PAGE_SIZE) {
			pgt_map_page(g_kernel_pagetable, va, pa, PTE_R |PTE_W| PTE_A | PTE_X| PTE_D); // BSS段：可读可写
	}
	//pagetable_dump(g_kernel_pagetable);
	
	// // 6. 映射MMIO区域（如果有需要）
	// // 例如UART、PLIC等外设的内存映射IO区域
	
	// sprint("kern_vm_init: complete\n");
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


static void memory_init(){
	init_page_manager();
	kern_vm_init();
	slab_init();

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
    memory_init();
    sig = 0;
  }else{
		while(sig){}
	}
  
  
  //sync_barrier(&sync_counter, NCPU);

  //  写入satp寄存器并刷新tlb缓存
  //    从这里开始，所有内存访问都通过MMU进行虚实转换

	pagetable_activate(g_kernel_pagetable);
  init_proc_pool();


  fs_init();

  sprint("Switch to user mode...\n");
  // the application code (elf) is first loaded into memory, and then put into execution
  // added @lab3_1
  insert_to_ready_queue( load_user_program() );
  schedule();

  // we should never reach here.
  return 0;
}
