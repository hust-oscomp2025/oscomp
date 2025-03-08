/*
 * Supervisor-mode startup codes
 */

#include <kernel/riscv.h>
#include <kernel/elf.h>
#include <kernel/process.h>

#include <kernel/sched.h>

#include <kernel/types.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/rfs.h>
#include <kernel/fs/ramdev.h>
#include <kernel/mm/pagetable.h>
#include <kernel/mm/kmalloc.h>

#include <util/string.h>

#include <spike_interface/spike_utils.h>

// 假设内核的虚拟地址和物理地址之间有一个固定偏移
// #define VIRTUAL_TO_PHYSICAL(vaddr) ((vaddr) - KERN_BASE + DRAM_BASE)
// #define PHYSICAL_TO_VIRTUAL(paddr) ((paddr) - DRAM_BASE + KERN_BASE)

#define VIRTUAL_TO_PHYSICAL(vaddr) ((uint64)(vaddr))
#define PHYSICAL_TO_VIRTUAL(paddr) ((uint64)(paddr))

// shift a physical address to the right place for a PTE.
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)



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
  free_mem_start_addr = ROUNDUP(g_kernel_end, PAGE_SIZE);


  // 重新计算g_mem_size以限制物理内存空间
  g_mem_size = ROUNDDOWN(MIN(PKE_MAX_ALLOWABLE_RAM, g_mem_size),PAGE_SIZE);
	assert(g_mem_size > pke_kernel_size);
  sprint("free physical memory address: [0x%lx, 0x%lx] \n", free_mem_start_addr,
    DRAM_BASE + g_mem_size - 1);
  


  // 初始化页管理子系统
  page_init(DRAM_BASE, g_mem_size, free_mem_start_addr);
  

  sprint("Physical memory manager initialization complete.\n");
}

//
// traverse the page table (starting from page_dir) to find the corresponding
// pte of va. returns: PTE (page table entry) pointing to va.
// 实际查询va对应的页表项的过程，可以选择是否在查询过程中为页中间目录和页表分配内存空间。
// 拿一个物理页当页表并不影响在页表中初始化这个物理页的映射关系。
pte_t *page_walk(pagetable_t page_dir, uint64 va, int alloc) {
  if (va >= MAXVA)
    panic("page_walk");

  // starting from the page directory
  pagetable_t pt = page_dir;

  // traverse from page directory to page table.
  // as we use risc-v sv39 paging scheme, there will be 3 layers: page dir,
  // page medium dir, and page table.
  for (int level = 2; level > 0; level--) {
    // macro "PX" gets the PTE index in page table of current level
    // "pte" points to the entry of current level
    pte_t *pte = pt + PX(level, va);

    // now, we need to know if above pte is valid (established mapping to a
    // phyiscal page) or not.
    if (*pte & PTE_V) { // PTE valid
      // phisical address of pagetable of next level
      pt = (pagetable_t)PTE2PA(*pte);
    } else { // PTE invalid (not exist).
      // allocate a page (to be the new pagetable), if alloc == 1
      if (alloc && ((pt = (pte_t *)alloc_page()->virtual_address) != 0)) {
        memset(pt, 0, PAGE_SIZE);
        // writes the physical address of newly allocated page to pte, to
        // establish the page table tree.
        *pte = PA2PTE(pt) | PTE_V;
      } else // returns NULL, if alloc == 0, or no more physical page remains
        return 0;
    }
  }

  // return a PTE which contains phisical address of a page
  return pt + PX(0, va);
}

//
// look up a virtual page address, return the physical page address or 0 if not
// mapped.
//
uint64 lookup_pa(pagetable_t pagetable, uint64 va) {
  pte_t *pte;
  uint64 pa;

  if (va >= MAXVA)
    return 0;

  pte = page_walk(pagetable, va, 0);
  if (pte == 0 || (*pte & PTE_V) == 0 ||
      ((*pte & PTE_R) == 0 && (*pte & PTE_W) == 0))
    return 0;
  pa = PTE2PA(*pte);

  return pa;
}


// 检查特定地址的映射
void check_address_mapping(pagetable_t pagetable, uint64 va) {
  uint64 vpn[3];
  vpn[2] = (va >> 30) & 0x1FF;
  vpn[1] = (va >> 21) & 0x1FF;
  vpn[0] = (va >> 12) & 0x1FF;
  
  sprint("Checking mapping for address 0x%lx (vpn: %d,%d,%d)\n", 
         va, vpn[2], vpn[1], vpn[0]);
  
  // 检查第一级
  pte_t *pte1 = &pagetable[vpn[2]];
  sprint("L1 PTE at 0x%lx: 0x%lx\n", (uint64)pte1, *pte1);
  if (!(*pte1 & PTE_V)) {
    sprint("  Invalid L1 entry!\n");
    return;
  }
  
  // 检查第二级
  pagetable_t pt2 = (pagetable_t)PTE2PA(*pte1);
  // 转换为虚拟地址以便访问
  pt2 = (pagetable_t)PHYSICAL_TO_VIRTUAL((uint64)pt2);
  pte_t *pte2 = &pt2[vpn[1]];
  sprint("L2 PTE at 0x%lx: 0x%lx\n", (uint64)pte2, *pte2);
  if (!(*pte2 & PTE_V)) {
    sprint("  Invalid L2 entry!\n");
    return;
  }
  
  // 检查第三级
  pagetable_t pt3 = (pagetable_t)PTE2PA(*pte2);
  pt3 = (pagetable_t)PHYSICAL_TO_VIRTUAL((uint64)pt3);
  pte_t *pte3 = &pt3[vpn[0]];
  sprint("L3 PTE at 0x%lx: 0x%lx\n", (uint64)pte3, *pte3);
  if (!(*pte3 & PTE_V)) {
    sprint("  Invalid L3 entry!\n");
    return;
  }
  
  // 分析最终PTE的权限
  sprint("  Physical addr: 0x%lx\n", PTE2PA(*pte3));
  sprint("  Permissions: %s%s%s%s%s%s%s\n",
         (*pte3 & PTE_R) ? "R" : "-",
         (*pte3 & PTE_W) ? "W" : "-",
         (*pte3 & PTE_X) ? "X" : "-",
         (*pte3 & PTE_U) ? "U" : "-",
         (*pte3 & PTE_G) ? "G" : "-",
         (*pte3 & PTE_A) ? "A" : "-",
         (*pte3 & PTE_D) ? "D" : "-");
}




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
	g_kernel_pagetable = (pagetable_t)0x80418000;
	memset(g_kernel_pagetable, 0, PAGE_SIZE);
	for (uint64 va = DRAM_BASE, pa = DRAM_BASE; 
			 va < DRAM_BASE + PKE_MAX_ALLOWABLE_RAM; 
			 va += PAGE_SIZE, pa += PAGE_SIZE) {
			pgt_map_page(g_kernel_pagetable, va, pa, PTE_R |PTE_W| PTE_A | PTE_X| PTE_D); // BSS段：可读可写
	}
	sprint("kernel pagetable %lx va is%lx\n",g_kernel_pagetable,pgt_lookuppa(g_kernel_pagetable,(uaddr)g_kernel_pagetable));
	sprint("kernel pagetable %lx va is%lx\n",0x8020f480,lookup_pa(g_kernel_pagetable,(uint64)0x8020f480));
	check_address_mapping(g_kernel_pagetable,(uint64)0x8020f498);
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
		sprint("Enter supervisor mode...\n");

    kern_vm_init();
    sig = 0;
  }
  while(sig){}
  
  //sync_barrier(&sync_counter, NCPU);

  //  写入satp寄存器并刷新tlb缓存
  //    从这里开始，所有内存访问都通过MMU进行虚实转换

	pagetable_activate(g_kernel_pagetable);
  sprint("s_start:pagetable_activate done\n");

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
