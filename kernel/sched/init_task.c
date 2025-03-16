#include <kernel/sched/process.h>
#include <kernel/proc_file.h>
#include <kernel/sched/sched.h>
#include <kernel/mm/kmalloc.h>



struct task_struct* alloc_init_task(){
	struct task_struct* task = alloc_empty_process();
	task->pid = 1;	// idle task是pid0
	task->state = TASK_RUNNING;
	// .prio        = MIN_PRIO,          // 通常是最低优先级
	// .static_prio = MIN_PRIO,
	task->flags = PF_KTHREAD;
	task->pagefault_disabled = 0;
  task->kstack = (uint64)alloc_kernel_stack();
  task->mm = NULL;
  task->fd_struct = alloc_pfm();
	return task;
}





// /*
//  * Set up the first task table, touch at your own risk!. Base=0,
//  * limit=0x1fffff (=2MB)
//  */
// struct task_struct init_task
// #ifdef CONFIG_ARCH_TASK_STRUCT_ON_STACK
// 	__init_task_data
// #endif
// 	__aligned(L1_CACHE_BYTES)
// = {
// #ifdef CONFIG_THREAD_INFO_IN_TASK
// 	.thread_info	= INIT_THREAD_INFO(init_task),
// 	.stack_refcount	= REFCOUNT_INIT(1),
// #endif
// 	.__state	= 0,
// 	.stack		= init_stack,
// 	.usage		= REFCOUNT_INIT(2),
// 	.flags		= PF_KTHREAD,
// 	.prio		= MAX_PRIO - 20,
// 	.static_prio	= MAX_PRIO - 20,
// 	.normal_prio	= MAX_PRIO - 20,
// 	.policy		= SCHED_NORMAL,
// 	.cpus_ptr	= &init_task.cpus_mask,
// 	.user_cpus_ptr	= NULL,
// 	.cpus_mask	= CPU_MASK_ALL,
// 	.nr_cpus_allowed= NR_CPUS,
// 	.mm		= NULL,
// 	.active_mm	= &init_mm,
// 	.restart_block	= {
// 		.fn = do_no_restart_syscall,
// 	},
// 	.se		= {
// 		.group_node 	= LIST_HEAD_INIT(init_task.se.group_node),
// 	},
// 	.rt		= {
// 		.run_list	= LIST_HEAD_INIT(init_task.rt.run_list),
// 		.time_slice	= RR_TIMESLICE,
// 	},
// 	.tasks		= LIST_HEAD_INIT(init_task.tasks),
// #ifdef CONFIG_SMP
// 	.pushable_tasks	= PLIST_NODE_INIT(init_task.pushable_tasks, MAX_PRIO),
// #endif
// #ifdef CONFIG_CGROUP_SCHED
// 	.sched_task_group = &root_task_group,
// #endif
// 	.ptraced	= LIST_HEAD_INIT(init_task.ptraced),
// 	.ptrace_entry	= LIST_HEAD_INIT(init_task.ptrace_entry),
// 	.real_parent	= &init_task,
// 	.parent		= &init_task,
// 	.children	= LIST_HEAD_INIT(init_task.children),
// 	.sibling	= LIST_HEAD_INIT(init_task.sibling),
// 	.group_leader	= &init_task,
// 	RCU_POINTER_INITIALIZER(real_cred, &init_cred),
// 	RCU_POINTER_INITIALIZER(cred, &init_cred),
// 	.comm		= INIT_TASK_COMM,
// 	.thread		= INIT_THREAD,
// 	.fs		= &init_fs,
// 	.files		= &init_files,
// #ifdef CONFIG_IO_URING
// 	.io_uring	= NULL,
// #endif
// 	.signal		= &init_signals,
// 	.sighand	= &init_sighand,
// 	.nsproxy	= &init_nsproxy,
// 	.pending	= {
// 		.list = LIST_HEAD_INIT(init_task.pending.list),
// 		.signal = {{0}}
// 	},
// 	.blocked	= {{0}},
// 	.alloc_lock	= __SPIN_LOCK_UNLOCKED(init_task.alloc_lock),
// 	.journal_info	= NULL,
// 	INIT_CPU_TIMERS(init_task)
// 	.pi_lock	= __RAW_SPIN_LOCK_UNLOCKED(init_task.pi_lock),
// 	.timer_slack_ns = 50000, /* 50 usec default slack */
// 	.thread_pid	= &init_struct_pid,
// 	.thread_group	= LIST_HEAD_INIT(init_task.thread_group),
// 	.thread_node	= LIST_HEAD_INIT(init_signals.thread_head),
// #ifdef CONFIG_AUDIT
// 	.loginuid	= INVALID_UID,
// 	.sessionid	= AUDIT_SID_UNSET,
// #endif
// #ifdef CONFIG_PERF_EVENTS
// 	.perf_event_mutex = __MUTEX_INITIALIZER(init_task.perf_event_mutex),
// 	.perf_event_list = LIST_HEAD_INIT(init_task.perf_event_list),
// #endif
// #ifdef CONFIG_PREEMPT_RCU
// 	.rcu_read_lock_nesting = 0,
// 	.rcu_read_unlock_special.s = 0,
// 	.rcu_node_entry = LIST_HEAD_INIT(init_task.rcu_node_entry),
// 	.rcu_blocked_node = NULL,
// #endif
// #ifdef CONFIG_TASKS_RCU
// 	.rcu_tasks_holdout = false,
// 	.rcu_tasks_holdout_list = LIST_HEAD_INIT(init_task.rcu_tasks_holdout_list),
// 	.rcu_tasks_idle_cpu = -1,
// #endif
// #ifdef CONFIG_TASKS_TRACE_RCU
// 	.trc_reader_nesting = 0,
// 	.trc_reader_special.s = 0,
// 	.trc_holdout_list = LIST_HEAD_INIT(init_task.trc_holdout_list),
// 	.trc_blkd_node = LIST_HEAD_INIT(init_task.trc_blkd_node),
// #endif
// #ifdef CONFIG_CPUSETS
// 	.mems_allowed_seq = SEQCNT_SPINLOCK_ZERO(init_task.mems_allowed_seq,
// 						 &init_task.alloc_lock),
// #endif
// #ifdef CONFIG_RT_MUTEXES
// 	.pi_waiters	= RB_ROOT_CACHED,
// 	.pi_top_task	= NULL,
// #endif
// 	INIT_PREV_CPUTIME(init_task)
// #ifdef CONFIG_VIRT_CPU_ACCOUNTING_GEN
// 	.vtime.seqcount	= SEQCNT_ZERO(init_task.vtime_seqcount),
// 	.vtime.starttime = 0,
// 	.vtime.state	= VTIME_SYS,
// #endif
// #ifdef CONFIG_NUMA_BALANCING
// 	.numa_preferred_nid = NUMA_NO_NODE,
// 	.numa_group	= NULL,
// 	.numa_faults	= NULL,
// #endif
// #if defined(CONFIG_KASAN_GENERIC) || defined(CONFIG_KASAN_SW_TAGS)
// 	.kasan_depth	= 1,
// #endif
// #ifdef CONFIG_KCSAN
// 	.kcsan_ctx = {
// 		.scoped_accesses	= {LIST_POISON1, NULL},
// 	},
// #endif
// #ifdef CONFIG_TRACE_IRQFLAGS
// 	.softirqs_enabled = 1,
// #endif
// #ifdef CONFIG_LOCKDEP
// 	.lockdep_depth = 0, /* no locks held yet */
// 	.curr_chain_key = INITIAL_CHAIN_KEY,
// 	.lockdep_recursion = 0,
// #endif
// #ifdef CONFIG_FUNCTION_GRAPH_TRACER
// 	.ret_stack		= NULL,
// 	.tracing_graph_pause	= ATOMIC_INIT(0),
// #endif
// #if defined(CONFIG_TRACING) && defined(CONFIG_PREEMPTION)
// 	.trace_recursion = 0,
// #endif
// #ifdef CONFIG_LIVEPATCH
// 	.patch_state	= KLP_UNDEFINED,
// #endif
// #ifdef CONFIG_SECURITY
// 	.security	= NULL,
// #endif
// #ifdef CONFIG_SECCOMP_FILTER
// 	.seccomp	= { .filter_count = ATOMIC_INIT(0) },
// #endif
// };
// EXPORT_SYMBOL(init_task);