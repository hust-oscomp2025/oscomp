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

/* 外部声明 */
extern void schedule(void);
extern void halt_cpu(void);
extern void scheduler_register_task(struct task_struct *tsk);

/* 内核全局内存管理结构和内核页表（假设已在其他地方初始化） */
extern struct mm_struct init_mm;
//extern pgd_t swapper_pg_dir[]; // 内核页目录

/*
 * 静态定义 idle_task，全局唯一的 idle 进程。
 * 注意：为了简单起见，只展示了关键字段的初始化，
 * 实际实现中还会有更多字段和 CPU 上下文信息。
 */
struct task_struct idle_task = {
    .pid = 0,
    .state = TASK_RUNNING, // Idle 进程始终处于可运行状态
    //.prio = MIN_PRIO,      // 设定最低优先级
    //.static_prio = MIN_PRIO,
    .flags = PF_KTHREAD,   // 标识为内核线程
    .mm = NULL,            // 内核线程无独立用户空间
		.trapframe = NULL,			// 无用户态
    .active_mm = &init_mm, // 共享全局内核内存管理结构
                           //.pgd = swapper_pg_dir, // 指向内核页表根
                           // 其他字段可根据需要补充初始化...
};


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

/*
 * init_idle_task - 初始化并注册 idle 进程
 *
 * 在内核启动过程中，此函数将被调用来完成 idle 进程的 CPU 上下文初始化，
 * 并将 idle 进程注册到调度器中，使其在必要时被调度执行。
 * 每个cpu都需要自己的idle任务
 * 
 */
void init_idle_task(void) {
	idle_task.ktrapframe = kmalloc(sizeof(struct trapframe));
	memset(&idle_task.ktrapframe,0,sizeof(struct trapframe));
  /*
   * 设置 idle 进程的 CPU 上下文，使得进程入口指向 idle_loop。
   * 这里假设 cpu_context 是一个保存寄存器和指令指针的结构，
   * 实际实现会依据具体平台定义不同。
   */
  idle_task.ktrapframe->epc = (unsigned long)idle_loop;
  // 可在此处补充其他 CPU 上下文的初始化操作

  /* 将 idle 进程注册到调度器中 */
  insert_to_ready_queue(&idle_task);

  sprint("Idle process (PID 0) initialized and registered.\n");
}


