/*
 * Supervisor-mode startup codes
 */

#include <kernel/elf.h>
#include <kernel/process.h>
#include <kernel/proc_file.h>
#include <kernel/riscv.h>

#include <kernel/sched/sched.h>

#include <kernel/fs/vfs.h>
#include <kernel/types.h>

#include <kernel/mm/mm_struct.h>
#include <kernel/mm/mmap.h>
#include <kernel/mm/pagetable.h>
#include <kernel/mm/slab.h>

#include <util/spinlock.h>
#include <util/string.h>

#include <spike_interface/spike_utils.h>

extern char _ftext[];
extern char _etext[];

extern char _fdata[];
extern char _edata[];


static void kernel_vm_init(void) {
  extern struct mm_struct init_mm;
	// 映射内核代码段和只读段
  mm_map_pages(&init_mm, _ftext, _ftext, _etext - _ftext,
               prot_to_type(PROT_READ | PROT_EXEC, 0), VMA_TEXT, MAP_POPULATE);

	// 映射内核HTIF段
  mm_map_pages(&init_mm, _ftext, _ftext, _etext - _ftext,
               prot_to_type(PROT_READ | PROT_WRITE, 0), VMA_DATA, MAP_POPULATE);

	// 映射内核数据段
  mm_map_pages(&init_mm, _fdata, _fdata, _edata - _fdata,
		prot_to_type(PROT_READ | PROT_WRITE, 0), VMA_DATA, MAP_POPULATE);

  // pagetable_dump(g_kernel_pagetable);

  // // 6. 映射MMIO区域（如果有需要）
  // // 例如UART、PLIC等外设的内存映射IO区域

  // sprint("kern_vm_init: complete\n");
}

// typedef union {
//   uint64 buf[MAX_CMDLINE_ARGS];
//   char *argv[MAX_CMDLINE_ARGS];
// } arg_buf;

//
// 初始化1号进程
//
static int load_init_process() {
  struct task_struct *task = alloc_init_task();

  task->mm = alloc_init_mm();
  kernel_vm_init();

  // 7. 设置文件描述符表
	task->pfiles = init_proc_file_management();
  // 8. 初始化标准文件描述符(stdin, stdout, stderr)
  // 这些会指向/dev/console或null设备
  setup_std_fds(task->pfiles);

  // 9. 初始化信号处理
  // task->sighand = alloc_sighand_struct();
  // if (!task->sighand) {
  //   free_files_struct(task->files);
  //   free_mm(task->mm);
  //   free_kernel_stack(task->stack);
  //   kfree(task);
  //   return NULL;
  // }
  extern struct task_struct idle_task;
  // 10. 初始化父进程指针(初始进程的父进程是内核idle进程)
  task->parent = &idle_task;

  // 11. 初始化子进程列表
  INIT_LIST_HEAD(&task->children);

  // // 12. 初始化CPU亲和性(初始进程可以在任何CPU上运行)
  // for (int i = 0; i < NR_CPUS; i++) {
  //   cpumask_set_cpu(i, &task->cpus_allowed);
  // }

  // 13. 初始化进程凭证(通常是root权限)
  task->uid = 0;
  task->gid = 0;
  task->euid = 0;
  task->egid = 0;

  // 14. 初始化进程命名空间(如果支持)
  // init_task_namespaces(task);

  // 15. 初始化其他可能的属性(根据系统需求)
  // init_task_timers(task);
  // init_task_io_accounting(task);

  return task;
}

static void memory_init() {
  init_page_manager();
  kernel_vm_init();
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

  init_fs();

  sprint("Switch to user mode...\n");
  // the application code (elf) is first loaded into memory, and then put into
  // execution added @lab3_1
  insert_to_ready_queue(load_init_process());
  schedule();

  // we should never reach here.
  return 0;
}
