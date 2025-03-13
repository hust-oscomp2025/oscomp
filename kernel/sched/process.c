/*
 * Utility functions for process management.
 *
 * Note: in Lab1, only one process (i.e., our user application) exists.
 * Therefore, PKE OS at this stage will set "current" to the loaded user
 * application, and also switch to the old "current" process after trap
 * handling.
 */

#include <kernel/config.h>
#include <kernel/elf.h>

#include <kernel/mm/mm_struct.h>
#include <kernel/mm/vma.h>
#include <kernel/mm/mmap.h>
#include <kernel/mm/kmalloc.h>

#include <kernel/sched/process.h>
#include <kernel/riscv.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/pid.h>
#include <kernel/semaphore.h>
#include <kernel/strap.h>

#include <spike_interface/spike_utils.h>
#include <util/string.h>

static void free_kernel_stack(uint64 kstack);

//
// switch to a user-mode process
//

extern void return_to_user(struct trapframe *, uint64 satp);


//
// allocate an empty process, init its vm space. returns the pointer to
// process strcuture. added @lab3_1
//
struct task_struct *alloc_process() {
  // locate the first usable process structure
  struct task_struct *ps = alloc_empty_process();
  ps->kstack = (uint64)alloc_kernel_stack();
  ps->trapframe = (struct trapframe*)kmalloc(sizeof(struct trapframe));
	ps->ktrapframe = NULL;
	ps->mm = user_alloc_mm();
  ps->pfiles = alloc_pfm();
	//ps->active_mm =ps->mm;
  // 分配内核栈
	ps->pid = pid_alloc();
	ps->state;
	ps->flags;
	ps->parent;
	ps->pagefault_disabled = 0;

	INIT_LIST_HEAD(&ps->children);
	INIT_LIST_HEAD(&ps->sibling);
	INIT_LIST_HEAD(&ps->queue_node);
  ps->tick_count = 0;

  // 创建信号量和初始化文件管理
  ps->sem_index = sem_new(0);	//这个信号量需要重写

  sprint("alloc_process: end.\n");
  return ps;
}

int free_process(struct task_struct *proc) {
  // 在exit中把进程的状态设成ZOMBIE，然后在父进程wait中调用这个函数，用于释放子进程的资源（待实现）
  // 由于代理内核的特殊机制，不做也不会造成内存泄漏（代填）

  return 0;
}

ssize_t do_wait(int pid) {
  // sprint("DEBUG LINE, pid = %d\n",pid);
  extern struct task_struct* procs[NPROC];
  int hartid = read_tp();
  // int child_found_flag = 0;
  if (pid == -1) {
    while (1) {
      for (int i = 0; i < NPROC; i++) {
        // sprint("DEBUG LINE\n");

        struct task_struct *p = procs[i];
        // sprint("p = 0x%lx,\n",p);
        if (p->parent != NULL && p->parent->pid == CURRENT->pid &&
            p->state & TASK_DEAD) {
          // sprint("DEBUG LINE\n");

          free_process(p);
          return i;
        }
      }
      // sprint("current->sem_index = %d\n",current->sem_index);
      sem_P(CURRENT->sem_index);
      // sprint("wait:return from blocking!\n");
    }
  }
  if (0 < pid && pid < NPROC) {
    // sprint("DEBUG LINE\n");

    struct task_struct *p = procs[pid];
    if (p->parent != CURRENT) {
      return -1;
    } else if (p->state & TASK_DEAD) {
      free_process(p);
      return pid;
    } else {
      sem_P(p->sem_index);
      // sprint("return from blocking!\n");

      return pid;
    }
  }
  return -1;
}



static void free_kernel_stack(uint64 kstack){
	kfree((uint64)ROUNDDOWN((uint64)kstack,PAGE_SIZE));
}



/**
 * 打印进程的内存布局信息，用于调试
 */
void print_proc_memory_layout(struct task_struct *proc) {
  if (!proc || !proc->mm)
    return;

  struct mm_struct *mm = proc->mm;

  sprint("Process %d memory layout:\n", proc->pid);
  sprint("  code: 0x%lx - 0x%lx\n", mm->start_code, mm->end_code);
  sprint("  data: 0x%lx - 0x%lx\n", mm->start_data, mm->end_data);
  sprint("  heap: 0x%lx - 0x%lx\n", mm->start_brk, mm->brk);
  sprint("  stack: 0x%lx - 0x%lx\n", mm->start_stack, mm->end_stack);

  sprint("  VMAs (%d):\n", mm->map_count);

  struct vm_area_struct *vma;
  list_for_each_entry(vma, &mm->vma_list, vm_list) {
    const char *type_str;
    switch (vma->vm_type) {
    case VMA_ANONYMOUS:
      type_str = "anon";
      break;
    case VMA_FILE:
      type_str = "file";
      break;
    case VMA_STACK:
      type_str = "stack";
      break;
    case VMA_HEAP:
      type_str = "heap";
      break;
    case VMA_TEXT:
      type_str = "code";
      break;
    case VMA_DATA:
      type_str = "data";
      break;
    default:
      type_str = "unknown";
      break;
    }

    char prot_str[8] = {0};
    if (vma->vm_prot & PROT_READ)
      strcat(prot_str, "r");
    if (vma->vm_prot & PROT_WRITE)
      strcat(prot_str, "w");
    if (vma->vm_prot & PROT_EXEC)
      strcat(prot_str, "x");

    sprint("    %s: 0x%lx - 0x%lx [%s] pages:%d\n", type_str, vma->vm_start,
           vma->vm_end, prot_str, vma->page_count);
  }
}