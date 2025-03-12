/*
 * idle_task.c - Idle 进程的初始化及 idle_loop 实现
 *
 * 参考 Linux 内核实现：
 *   - Idle 进程（PID 0）是在内核启动阶段静态分配的，
 *     它不通过 fork 创建，而是在编译时直接定义。
 *   - Idle 进程负责在系统无其他可运行任务时，进入低功耗状态。
 */

#include <spike_interface/spike_utils.h>
// printk、KERN_INFO 等
#include <kernel/sched/sched.h> // task_struct 定义，TASK_RUNNING, PF_KTHREAD 等
//#include <linux/init.h>         // __init 宏
#include <util/spinlock.h>      // 锁相关函数
#include <kernel/mm/kmalloc.h>
#include <util/string.h>
#include <util/list.h>

/* 外部声明 */
extern void schedule(void);
extern void scheduler_register_task(struct task_struct *tsk);


static inline void halt_cpu(void) {
	__asm__ volatile ("wfi" ::: "memory");
}

/*
 * idle_loop - Idle 进程的主循环
 *
 * 当系统中没有其他可运行的进程时，idle_loop 将被调度执行，
 * 它会不断调用 schedule() 尝试切换到其他任务，
 * 若无任务可运行，则调用 halt_cpu() 让 CPU 进入低功耗等待状态。
 */
void idle_loop(void) {
  while (1) {
    schedule(); // 尝试切换到更高优先级任务
    halt_cpu(); // 没有任务时进入低功耗等待（如 HLT 指令）
  }
}



/* 内核全局内存管理结构和内核页表（假设已在其他地方初始化） */
//extern pgd_t swapper_pg_dir[]; // 内核页目录

/*
 * 静态定义 idle_task，全局唯一的 idle 进程。
 * 注意：为了简单起见，只展示了关键字段的初始化，
 * 实际实现中还会有更多字段和 CPU 上下文信息。
 * 这里特指0号idle_task
 */
struct task_struct idle_task;
//struct proc_file_management kernel_file_management;
/*
 * init_idle_task - 初始化并注册 idle 进程
 *
 * 在内核启动过程中，此函数将被调用来完成 idle 进程的 CPU 上下文初始化，
 * 并将 idle 进程注册到调度器中，使其在必要时被调度执行。
 * 每个cpu都需要自己的idle任务
 * 
 */
void init_idle_task(void) {
	idle_task.kstack = (uint64)alloc_kernel_stack();
	idle_task.trapframe = NULL;

	idle_task.ktrapframe = kmalloc(sizeof(struct trapframe));
	memset(&idle_task.ktrapframe,0,sizeof(struct trapframe));
  idle_task.ktrapframe->epc = (unsigned long)idle_loop;

	extern struct mm_struct init_mm;
	idle_task.mm = &init_mm;
  idle_task.pfiles = NULL;




	idle_task.pid = 0;

	idle_task.state = TASK_RUNNING; // Idle 进程始终处于可运行状态
	idle_task.flags = PF_KTHREAD;
	idle_task.parent;
	INIT_LIST_HEAD(&idle_task.children);
	INIT_LIST_HEAD(&idle_task.sibling);
	INIT_LIST_HEAD(&idle_task.queue_node);
  idle_task.tick_count = 0;

  //ps->sem_index = sem_new(0);	//这个信号量需要重写

  /* 将 idle 进程注册到调度器中 */
  insert_to_ready_queue(&idle_task);

  sprint("Idle process (PID 0) initialized and registered.\n");
}







