/*
 * Supervisor-mode startup codes
 */

#include <kernel/elf.h>
#include <kernel/process.h>
#include <kernel/riscv.h>

#include <kernel/sched.h>

#include <kernel/fs/vfs.h>
#include <kernel/types.h>

#include <kernel/mm/pagetable.h>
#include <kernel/mm/slab.h>

#include <util/spinlock.h>
#include <util/string.h>

#include <spike_interface/spike_utils.h>

extern char _etext[];
extern char _edata[];
static void kern_vm_init(void) {
  // // 分配一个页作为内核的根页表
  // g_kernel_pagetable = alloc_page()->virtual_address;
  // memset(g_kernel_pagetable, 0, PAGE_SIZE);
  // sprint("kern_vm_init: global pagetable page address: %lx\n",
  // g_kernel_pagetable); sprint("kern_vm_init: pagetable memory address:
  // %lx\n", &g_kernel_pagetable);

  // // 获取页表的物理地址
  // uint64 pagetable_pa = VIRTUAL_TO_PHYSICAL(g_kernel_pagetable);

  // // 1. 首先映射页表自身，确保在启用 MMU 后还能访问页表
  // pgt_map_page(g_kernel_pagetable, (uint64)g_kernel_pagetable, pagetable_pa,
  // PTE_R | PTE_W);

  // // 2. 映射内核代码段 (.text)
  // uint64 text_start = KERN_BASE;
  // uint64 text_end = ROUNDUP((uint64)_etext, PAGE_SIZE);
  // sprint("kern_vm_init: mapping text section [%lx-%lx]\n", text_start,
  // text_end);

  // for (uint64 va = text_start, pa = VIRTUAL_TO_PHYSICAL(va);
  // 		 va < text_end;
  // 		 va += PAGE_SIZE, pa += PAGE_SIZE) {
  // 		pgt_map_page(g_kernel_pagetable, va, pa, PTE_R | PTE_X); //
  // 代码段：可读可执行
  // }

  // // 3. 映射内核数据段 (.data)
  // uint64 data_start = ROUNDDOWN((uint64)_etext, PAGE_SIZE);
  // uint64 data_end = ROUNDUP((uint64)_edata, PAGE_SIZE);
  // sprint("kern_vm_init: mapping data section [%lx-%lx]\n", data_start,
  // data_end);

  // for (uint64 va = data_start, pa = VIRTUAL_TO_PHYSICAL(va);
  // 		 va < data_end;
  // 		 va += PAGE_SIZE, pa += PAGE_SIZE) {
  // 		pgt_map_page(g_kernel_pagetable, va, pa, PTE_R | PTE_W); //
  // 数据段：可读可写
  // }

  // // 4. 映射内核 BSS 段
  // uint64 bss_start = ROUNDDOWN((uint64)_edata, PAGE_SIZE);
  // uint64 bss_end = ROUNDUP((uint64)_end, PAGE_SIZE);
  // sprint("kern_vm_init: mapping bss section [%lx-%lx]\n", bss_start,
  // bss_end);

  // for (uint64 va = bss_start, pa = VIRTUAL_TO_PHYSICAL(va);
  // 		 va < bss_end;
  // 		 va += PAGE_SIZE, pa += PAGE_SIZE) {
  // 		pgt_map_page(g_kernel_pagetable, va, pa, PTE_R | PTE_W); //
  // BSS段：可读可写
  // }
  // g_kernel_pagetable = alloc_page()->virtual_address;
  g_kernel_pagetable = alloc_page()->virtual_address;
  memset(g_kernel_pagetable, 0, PAGE_SIZE);
  for (uint64 va = KERN_BASE, pa = KERN_BASE;
       va < DRAM_BASE + PKE_MAX_ALLOWABLE_RAM;
       va += PAGE_SIZE, pa += PAGE_SIZE) {
    pgt_map_page(g_kernel_pagetable, va, pa,
                 PTE_R | PTE_W | PTE_A | PTE_X | PTE_D); // BSS段：可读可写
  }
  // pagetable_dump(g_kernel_pagetable);

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
// 初始化0号进程
// 
static int load_init_process() {
  struct task_struct *task = alloc_init_task();
  if (!task)
    return NULL;

  // 2. 清零任务结构
  memset(task, 0, sizeof(struct task_struct));

  // 3. 初始化基本任务属性
  task->pid = 1;                      // 初始进程PID为1
  task->status = TASK_UNINTERRUPTIBLE; // 初始状态为不可中断(防止被意外终止)
  task->flags = PF_KTHREAD; // 先标记为内核线程，之后会更改为用户进程
  //task->priority = DEFAULT_PRIO; // 我们还没有引入进程优先级

  // 4. 初始化时间片和调度相关信息
  //task->time_slice = DEFAULT_TIMESLICE;
  //task->sched_class = &fair_sched_class; // 使用默认的公平调度类
    // 5. 分配并初始化内核栈
    task->kstack = alloc_kernel_stack();
    if (!task->stack) {
        kfree(task);
        return NULL;
    }
    
    // 6. 初始化地址空间(mm结构)
    task->mm = alloc_mm();
    if (!task->mm) {
        free_kernel_stack(task->stack);
        kfree(task);
        return NULL;
    }
    
    // 7. 设置文件描述符表
    task->files = alloc_files_struct();
    if (!task->files) {
        free_mm(task->mm);
        free_kernel_stack(task->stack);
        kfree(task);
        return NULL;
    }
    
    // 8. 初始化标准文件描述符(stdin, stdout, stderr)
    // 这些会指向/dev/console或null设备
    setup_std_fds(task->files);
    
    // 9. 初始化信号处理
    task->sighand = alloc_sighand_struct();
    if (!task->sighand) {
        free_files_struct(task->files);
        free_mm(task->mm);
        free_kernel_stack(task->stack);
        kfree(task);
        return NULL;
    }
    
    // 10. 初始化父进程指针(初始进程的父进程是内核idle进程)
    task->parent = &init_idle_task;
    
    // 11. 初始化子进程列表
    INIT_LIST_HEAD(&task->children);
    
    // 12. 初始化CPU亲和性(初始进程可以在任何CPU上运行)
    for (int i = 0; i < NR_CPUS; i++) {
        cpumask_set_cpu(i, &task->cpus_allowed);
    }
    
    // 13. 初始化进程凭证(通常是root权限)
    task->uid = 0;
    task->gid = 0;
    task->euid = 0;
    task->egid = 0;
    
    // 14. 初始化进程命名空间(如果支持)
    init_task_namespaces(task);
    
    // 15. 初始化其他可能的属性(根据系统需求)
    init_task_timers(task);
    init_task_io_accounting(task);
    
    return task;
}

static void memory_init() {
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
  if (hartid == 0) {
    memory_init();
    sig = 0;
  } else {
    while (sig) {
    }
  }

  // sync_barrier(&sync_counter, NCPU);

  //  写入satp寄存器并刷新tlb缓存
  //    从这里开始，所有内存访问都通过MMU进行虚实转换

  pagetable_activate(g_kernel_pagetable);
  init_scheduler();

  fs_init();

  sprint("Switch to user mode...\n");
  // the application code (elf) is first loaded into memory, and then put into
  // execution added @lab3_1
  insert_to_ready_queue(load_init_process());
  schedule();

  // we should never reach here.
  return 0;
}
