#ifndef _PROC_H_
#define _PROC_H_

#include <kernel/riscv.h>
#include <kernel/proc_file.h>




// riscv-pke kernel supports at most 32 processes
#define NPROC 32


struct trapframe {
  // space to store context (all common registers)
  /* offset:0   */ riscv_regs regs;

  // process's "user kernel" stack
  /* offset:248 */ uint64 kernel_sp;
  // pointer to smode_trap_handler
  /* offset:256 */ uint64 kernel_trap;
  // saved user process counter
  /* offset:264 */ uint64 epc;

  // kernel page table. added @lab2_1
  /* offset:272 */ uint64 kernel_satp;
	// kernel scheduler, added @lab3_challenge2
	/* offset:280 */ uint64 kernel_schedule;
};

/* Linux内核进程flags定义表 */

/* 进程标志(task->flags) */
#define PF_IDLE                 0x00000002  // 空闲进程
#define PF_EXITING              0x00000004  // 进程正在退出
#define PF_EXITPIDONE           0x00000008  // 进程退出通知已经发送
#define PF_VCPU                 0x00000010  // 虚拟CPU进程
#define PF_WQ_WORKER            0x00000020  // 工作队列worker线程
#define PF_FORKNOEXEC           0x00000040  // fork后还未执行exec
#define PF_MCE_PROCESS          0x00000080  // 处理机器检查异常的进程
#define PF_SUPERPRIV            0x00000100  // 用超级用户权限
#define PF_DUMPCORE             0x00000200  // 进程dump core
#define PF_SIGNALED             0x00000400  // 进程因信号而终止
#define PF_MEMALLOC             0x00000800  // 允许分配内存
#define PF_NPROC_EXCEEDED       0x00001000  // 设置限制：不允许fork
#define PF_USED_MATH            0x00002000  // 进程使用FPU
#define PF_USED_ASYNC           0x00004000  // 进程使用异步I/O
#define PF_NOFREEZE             0x00008000  // 进程不应被冻结
#define PF_FROZEN               0x00010000  // 进程已被冻结
#define PF_FSTRANS              0x00020000  // 文件系统锁
#define PF_KSWAPD               0x00040000  // 内存回收(kswapd)进程
#define PF_MEMALLOC_NOFS        0x00080000  // 内存分配时不要使用FS
#define PF_LESS_THROTTLE        0x00100000  // 减少I/O节流
#define PF_KTHREAD              0x00200000  // 内核线程标志
#define PF_RANDOMIZE            0x00400000  // 地址空间随机化
#define PF_SWAPWRITE            0x00800000  // 写交换区
#define PF_NO_SETAFFINITY       0x04000000  // 不允许设置CPU亲和性
#define PF_MCE_EARLY            0x08000000  // 早期机器检查错误处理
#define PF_MUTEX_TESTER         0x20000000  // 互斥测试
#define PF_FREEZER_SKIP         0x40000000  // 跳过冻结
#define PF_SUSPEND_TASK         0x80000000  // 系统挂起任务

/* 组合标志 */
#define PF_MEMALLOC_FLAGS (PF_MEMALLOC | PF_MEMALLOC_NOFS)  // 内存分配相关标志

/* 示例使用 */
// task->flags = PF_KTHREAD;              // 设置为内核线程
// task->flags |= PF_NOFREEZE;            // 添加不允许冻结标志
// task->flags &= ~PF_KTHREAD;            // 清除内核线程标志
// if (task->flags & PF_EXITING) {...}    // 检查进程是否正在退出



/* Linux内核进程状态定义表 */

/* 基本进程状态 */
#define TASK_RUNNING                    0  // 进程正在运行或在运行队列中等待被调度
#define TASK_INTERRUPTIBLE              1  // 进程处于可中断的睡眠状态，可以被信号唤醒
#define TASK_UNINTERRUPTIBLE            2  // 进程处于不可中断的睡眠状态，不响应信号，直到某个条件满足
#define __TASK_STOPPED                  4  // 进程已停止执行，通常因为收到SIGSTOP信号
#define __TASK_TRACED                   8  // 进程正在被调试(通过ptrace)

/* 特殊状态标志 */
#define TASK_PARKED                   0x10  // 进程已被临时挂起(用于功耗管理) 
#define TASK_DEAD                     0x20  // 进程已终止，正在被清理
#define TASK_WAKEKILL                 0x40  // 进程会在唤醒时被杀死(用于SIGKILL处理) 
#define TASK_WAKING                   0x80  // 进程正在被唤醒过程中
#define TASK_NOLOAD                  0x100  // 不计入负载统计的任务
#define TASK_NEW                     0x200  // 刚创建，尚未完全初始化的进程
#define TASK_STATE_MAX               0x400  // 状态值的最大边界

/* 复合状态(组合状态) */
#define TASK_KILLABLE        (TASK_WAKEKILL | TASK_UNINTERRUPTIBLE)  // 不可中断但可被SIGKILL终止
#define TASK_STOPPED         (TASK_WAKEKILL | __TASK_STOPPED)        // 已停止并会在唤醒时被杀死 
#define TASK_TRACED          (TASK_WAKEKILL | __TASK_TRACED)         // 被跟踪并会在唤醒时被杀死

/* 状态掩码 */
#define TASK_STATE_TO_CHAR_STR "RSDTtXZPI"  // 状态显示字符

// types of a segment
enum fork_choice {
  FORK_MAP = 0,   // 直接映射代码段
  FORK_COPY, // 直接复制所有数据
	FORK_COW,
};


// the extremely simple definition of process, used for begining labs of PKE
struct task_struct {
  uint64 kstack;		// 分配一个页面当内核栈，注意内核栈的范围是[kstack-PAGE_SIZE, kstack)
  struct trapframe* trapframe;
	struct trapframe* ktrapframe;

  struct mm_struct *mm;

  // heap management
  // process_heap_manager user_heap;

  // process id
  uint64 pid;
  // process status
  unsigned int status;
	unsigned int flags;
  // parent process
  struct task_struct *parent;
  // next queue element
  struct task_struct *queue_next;

  // accounting. added @lab3_3
  int tick_count;

	int sem_index;
  // file system. added @lab4_1
  proc_file_management *pfiles;

};
struct task_struct* alloc_init_task();
// switch to run user app
void switch_to(struct task_struct*);
void init_proc_pool();
struct task_struct* alloc_process();
int free_process( struct task_struct* proc );

// fork a child from parent
int do_fork(struct task_struct* parent);
int do_exec(void *path);
ssize_t do_wait(int pid);
// current_percpu points to the currently running user-mode application.

// current_percpu running process
// extern process* current_percpu[NCPU];
extern struct task_struct* current_percpu[NCPU];
#define CURRENT (current_percpu[read_tp()])

#endif
