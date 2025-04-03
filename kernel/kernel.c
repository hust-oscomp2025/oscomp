/*
 * Supervisor-mode startup codes
 */

#include <kernel/elf.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/mm/mm_struct.h>
#include <kernel/mm/mmap.h>
#include <kernel/mm/pagetable.h>
#include <kernel/riscv.h>
#include <kernel/sched/process.h>
#include <kernel/sched/sched.h>
#include <kernel/util/print.h>
#include <kernel/types.h>
#include <kernel/util.h>
#include <kernel/vfs.h>
#include <kernel/syscall/syscall.h>

static void kernel_vm_init(void) {
	sprint("kernel_vm_init: start\n");
	// extern struct mm_struct init_mm;
	//  映射内核代码段和只读段
	g_kernel_pagetable = (pagetable_t)alloc_page()->paddr;
	// init_mm.pagetable = g_kernel_pagetable;
	//  之后它会被加入内核的虚拟空间，先临时用一个页
	memset(g_kernel_pagetable, 0, PAGE_SIZE);

	extern char _ftext[], _etext[], _fdata[], _end[];
	// sprint("_etext=%lx,_ftext=%lx\n", _etext, _ftext);

	pgt_map_pages(g_kernel_pagetable, (uint64)_ftext, (uint64)_ftext, (uint64)(_etext - _ftext), prot_to_type(PROT_READ | PROT_EXEC, 0));

	// 映射内核HTIF段
	pgt_map_pages(g_kernel_pagetable, (uint64)_etext, (uint64)_etext, (uint64)(_fdata - _etext), prot_to_type(PROT_READ | PROT_WRITE, 0));

	// 映射内核数据段
	pgt_map_pages(g_kernel_pagetable, (uint64)_fdata, (uint64)_fdata, (uint64)(_end - _fdata), prot_to_type(PROT_READ | PROT_WRITE, 0));
	// 映射内核数据段
	pgt_map_pages(g_kernel_pagetable, (uint64)_fdata, (uint64)_fdata, (uint64)(_end - _fdata), prot_to_type(PROT_READ | PROT_WRITE, 0));

	extern uint64 mem_size;
	// 对于剩余的物理内存空间做直接映射
	pgt_map_pages(g_kernel_pagetable, (uint64)_end, (uint64)_end, DRAM_BASE + mem_size, prot_to_type(PROT_READ | PROT_WRITE, 0));
	// // satp不通过这层映射找g_kernel_pagetable，但是为了维护它，也需要做一个映射
	// pgt_map_pages(g_kernel_pagetable, (uint64)g_kernel_pagetable,
	//               (uint64)g_kernel_pagetable, PAGE_SIZE,
	//               prot_to_type(PROT_READ | PROT_WRITE, 0));

	// 映射内核栈
	// pgt_map_pages(init_mm.pagetable, (uint64)init_mm.pagetable, )

	// pagetable_dump(g_kernel_pagetable);

	// // 6. 映射MMIO区域（如果有需要）
	// // 例如UART、PLIC等外设的内存映射IO区域

	sprint("kern_vm_init: complete\n");
}

/**
 * setup_init_fds - Set up standard file descriptors for init process
 * @init_task: Pointer to the init task structure
 *
 * Sets up standard input, output, and error for the init process.
 * This must be called before the init process starts executing.
 *
 * Returns: 0 on success, negative error code on failure
 */
int32 setup_init_fds(struct task_struct* init_task) {
	int32 fd, console_fd;
	struct task_struct* saved_task = current_task();

	// Temporarily set current to init task
	set_current_task(init_task);

	// Set up stdin, stdout, stderr
	for (fd = 0; fd < 3; fd++) {
		if(fd != do_open("/dev/console", O_RDWR, 0)) {
			sprint("Failed to open /dev/console for fd %d\n", fd);
			set_current_task(saved_task);
			return -1;
		}
	}

	// Restore original task
	set_current_task(saved_task);
	return 0;
}

int32 create_init_process(void) {
	struct task_struct* init_task;
	int32 error = -1;

	// Create the init process task structure
	init_task = alloc_process(); // No parent for init
	if (!init_task) return -ENOMEM;

	// Set up process ID and other basic attributes
	init_task->pid = 1;
	init_task->parent = NULL; // No parent

	setup_init_fds(init_task);
	// Load the init binary
	error = load_init_binary(init_task, "/sbin/init");
	if (error) goto fail_exec;

	// Add to scheduler queue and start running
	insert_to_ready_queue(init_task);

	// we should never reach here.
	return 0;

fail_exec:
	// Clean up file descriptors
	for (int fd = 0; fd < init_task->fdtable->max_fds; fd++) {
		if (init_task->fdtable->fd_array[fd]) fdtable_closeFd(init_task->fdtable, fd);
	}
fail_fds:
	fs_struct_unref(init_task->fs);
fail_fs:
	free_process(init_task);
	return error;
}

//
// s_start: S-mode entry point of riscv-pke OS kernel.
//
volatile static int32 sig = 1;
int32 s_start(void) {
	extern void init_idle_task(void);

	sprint("Enter supervisor mode...\n");
	write_csr(satp, 0);

	int32 hartid = read_tp();
	if (hartid == 0) {
		init_page_manager();
		kernel_vm_init();
		pagetable_activate(g_kernel_pagetable);
		create_init_mm();
		kmem_init();
		init_scheduler();

		init_idle_task();
		// kmalloc在形式上需要使用init_mm的“用户虚拟空间分配器”
		// 所以我们在启用kmalloc之前，需要先初始化0号进程

		init_scheduler();
		vfs_init();
		sig = 0;
	} else {
		while (sig) {}
		pagetable_activate(g_kernel_pagetable);
	}

	// sync_barrier(&sync_counter, NCPU);

	//  写入satp寄存器并刷新tlb缓存
	//    从这里开始，所有内存访问都通过MMU进行虚实转换

	sprint("Switch to user mode...\n");
	// the application code (elf) is first loaded into memory, and then put into
	// execution added @lab3_1
	create_init_process();

	// we should never reach here.
	return 0;
}
