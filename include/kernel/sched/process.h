#ifndef _PROC_H_
#define _PROC_H_

#include <kernel/riscv.h>
#include <kernel/trapframe.h>
#include <kernel/sched/signal.h>
#include <kernel/util/list.h>

struct fdtable;
struct fs_struct;

/* Linux内核进程flags定义表 */

/* 进程标志(task->flags) */
#define PF_IDLE 0x00000002           // 空闲进程
#define PF_EXITING 0x00000004        // 进程正在退出
#define PF_EXITPIDONE 0x00000008     // 进程退出通知已经发送
#define PF_VCPU 0x00000010           // 虚拟CPU进程
#define PF_WQ_WORKER 0x00000020      // 工作队列worker线程
#define PF_FORKNOEXEC 0x00000040     // fork后还未执行exec
#define PF_MCE_PROCESS 0x00000080    // 处理机器检查异常的进程
#define PF_SUPERPRIV 0x00000100      // 用超级用户权限
#define PF_DUMPCORE 0x00000200       // 进程dump core
#define PF_SIGNALED 0x00000400       // 进程因信号而终止
#define PF_MEMALLOC 0x00000800       // 允许分配内存
#define PF_NPROC_EXCEEDED 0x00001000 // 设置限制：不允许fork
#define PF_USED_MATH 0x00002000      // 进程使用FPU
#define PF_USED_ASYNC 0x00004000     // 进程使用异步I/O
#define PF_NOFREEZE 0x00008000       // 进程不应被冻结
#define PF_FROZEN 0x00010000         // 进程已被冻结
#define PF_FSTRANS 0x00020000        // 文件系统锁
#define PF_KSWAPD 0x00040000         // 内存回收(kswapd)进程
#define PF_MEMALLOC_NOFS 0x00080000  // 内存分配时不要使用FS
#define PF_LESS_THROTTLE 0x00100000  // 减少I/O节流
#define PF_KTHREAD 0x00200000        // 内核线程标志
#define PF_RANDOMIZE 0x00400000      // 地址空间随机化
#define PF_SWAPWRITE 0x00800000      // 写交换区
#define PF_NO_SETAFFINITY 0x04000000 // 不允许设置CPU亲和性
#define PF_MCE_EARLY 0x08000000      // 早期机器检查错误处理
#define PF_MUTEX_TESTER 0x20000000   // 互斥测试
#define PF_FREEZER_SKIP 0x40000000   // 跳过冻结
#define PF_SUSPEND_TASK 0x80000000   // 系统挂起任务

/* 组合标志 */
#define PF_MEMALLOC_FLAGS (PF_MEMALLOC | PF_MEMALLOC_NOFS) // 内存分配相关标志

/* 示例使用 */
// task->flags = PF_KTHREAD;              // 设置为内核线程
// task->flags |= PF_NOFREEZE;            // 添加不允许冻结标志
// task->flags &= ~PF_KTHREAD;            // 清除内核线程标志
// if (task->flags & PF_EXITING) {...}    // 检查进程是否正在退出

/*
 * Task state bitmask. NOTE! These bits are also
 * encoded in fs/proc/array.c: get_task_state().
 *
 * We have two separate sets of flags: task->state
 * is about runnability, while task->exit_state are
 * about the task exiting. Confusing, but this way
 * modifying one set can't modify the other one by
 * mistake.
 */

/* Used in tsk->state: */
#define TASK_RUNNING 0x00000000
// TASK_RUNNING同时表示就绪状态和运行状态，就绪状态在进程队列中，加以区分。
#define TASK_INTERRUPTIBLE 0x00000001
#define TASK_UNINTERRUPTIBLE 0x00000002
#define __TASK_STOPPED 0x00000004
#define __TASK_TRACED 0x00000008
/* Used in tsk->exit_state: */
#define EXIT_DEAD 0x00000010
#define EXIT_ZOMBIE 0x00000020
#define EXIT_TRACE (EXIT_ZOMBIE | EXIT_DEAD)
/* Used in tsk->state again: */
#define TASK_PARKED 0x00000040
#define TASK_DEAD 0x00000080
#define TASK_WAKEKILL 0x00000100
#define TASK_WAKING 0x00000200
#define TASK_NOLOAD 0x00000400
#define TASK_NEW 0x00000800
#define TASK_RTLOCK_WAIT 0x00001000
#define TASK_FREEZABLE 0x00002000
// #define __TASK_FREEZABLE_UNSAFE	       (0x00004000 *
// IS_ENABLED(CONFIG_LOCKDEP))
#define TASK_FROZEN 0x00008000
#define TASK_STATE_MAX 0x00010000

#define TASK_ANY (TASK_STATE_MAX - 1)
/* 复合状态(组合状态) */
#define TASK_KILLABLE (TASK_WAKEKILL | TASK_UNINTERRUPTIBLE) // 不可中断但可被SIGKILL终止
#define TASK_STOPPED (TASK_WAKEKILL | __TASK_STOPPED)        // 已停止并会在唤醒时被杀死
#define TASK_TRACED (TASK_WAKEKILL | __TASK_TRACED)          // 被跟踪并会在唤醒时被杀死

/* 状态掩码 */
#define TASK_STATE_TO_CHAR_STR "RSDTtXZPI" // 状态显示字符

// types of a segment
enum fork_choice {
	FORK_MAP = 0, // 直接映射代码段
	FORK_COPY,    // 直接复制所有数据
	FORK_COW,
};

struct task_struct {
	uint64 kstack; // 分配一个页面当内核栈，注意内核栈的范围是[kstack-PAGE_SIZE,
	               // kstack)
	struct trapframe* trapframe;
	struct trapframe* ktrapframe;

	struct mm_struct* mm;
	// struct mm_struct *active_mm;
	//  我们不需要使用复杂的active_mm特性
	/* File system info */
	struct fs_struct* fs;
	struct fdtable* fdtable;

	// process id
	pid_t pid;
	// process state
	uint32 state;
	uint32 flags;
	// parent process
	struct task_struct* parent;
	struct list_head children;
	struct list_head sibling;
	// ready queue
	struct list_head ready_queue_node;

	// accounting. added @lab3_3
	int32 tick_count;

	// int32 sem_index;

	// int32 pagefault_disabled;

	/* Signal handling */
	sigset_t pending;                // Pending signals
	sigset_t blocked;                // Blocked signals (signal mask)
	sigset_t saved_sigmask;          // Saved signal mask for sigsuspend
	struct sigaction sighand[_NSIG]; // Signal handlers

	uint64 signal_flags; // Signal-related flags
	int32 exit_signal;            // Signal delivered to parent on exit

	uid_t uid;
	uid_t euid;
	gid_t gid;
	gid_t egid;
	// 目前还用不上这些字段
};
struct task_struct* alloc_init_task();

struct task_struct* alloc_process();
int32 free_process(struct task_struct* proc);

// fork a child from parent
int32 do_fork(struct task_struct* parent);
int32 do_exec(uint64 path);
ssize_t do_wait(int32 pid);
int32 current_is_in_group(gid_t gid);

/**
 * 打印进程的内存布局信息，用于调试
 */
void print_proc_memory_layout(struct task_struct* proc);


/**
 * Helper function to check if current task belongs to the given group
 */
static inline int current_group_matches(gid_t gid) {
    int i;
    
    // /* Check primary group */
    // if (current_task()->gid == gid)
    //     return 1;
        
    // /* Check supplementary groups */
    // for (i = 0; i < current_task()->ngroups; i++) {
    //     if (current_task()->groups[i] == gid)
    //         return 1;
    // }
    
    // return 0;
	return 1;
}

#endif
