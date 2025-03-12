#include <kernel/mm/page.h>
#include <kernel/mm/mm_struct.h>
#include <kernel/mm/pagetable.h>
#include <util/string.h>
#include <spike_interface/spike_utils.h>

struct mm_struct init_mm;


// 在s_start中调用
void create_init_mm() {
	sprint("create_init_mm: start\n");
  memset(&init_mm, 0, sizeof(init_mm));

  //init_mm.pagetable = alloc_page()->virtual_address;
  init_mm.pagetable = g_kernel_pagetable;

  INIT_LIST_HEAD(&init_mm.vma_list);
  init_mm.map_count = 0;

	// 因为实际sv39地址空间很大，所以说内核的虚拟地址可以都在物理地址区间之后分配。
	// 这样就不会有地址映射上的冲突了，直接把物理内存当做“内核vma”设定。
  init_mm.start_code;
  init_mm.end_code; // 代码段范围

  init_mm.start_data;
  init_mm.end_data; // 数据段范围


	extern uint64 mem_size;
  init_mm.start_brk = DRAM_BASE;
  init_mm.brk = DRAM_BASE + mem_size; 					
	// 内核mm中的brk字段，在形式上用来设置do_mmap的起始地址

  init_mm.start_stack;
  init_mm.end_stack; 	// 栈范围，内核mm不需要使用这个字段。

	INIT_LIST_HEAD(&init_mm.vma_list);
  init_mm.map_count = 0;


	spinlock_init(&init_mm.mm_lock);
	atomic_set(&init_mm.mm_users,0);
	atomic_set(&init_mm.mm_count,0);
	sprint("create_init_mm: complete.\n");
}

// /*
//  * For dynamically allocated mm_structs, there is a dynamically sized cpumask
//  * at the end of the structure, the size of which depends on the maximum CPU
//  * number the system can see. That way we allocate only as much memory for
//  * mm_cpumask() as needed for the hundreds, or thousands of processes that
//  * a system typically runs.
//  *
//  * Since there is only one init_mm in the entire system, keep it simple
//  * and size this cpu_bitmask to NR_CPUS.
//  */
// struct mm_struct init_mm = {
//     .mm_mt = MTREE_INIT_EXT(mm_mt, MM_MT_FLAGS, init_mm.mmap_lock),
//     .pgd = swapper_pg_dir,
//     .mm_users = ATOMIC_INIT(2),
//     .mm_count = ATOMIC_INIT(1),
//     .write_protect_seq = SEQCNT_ZERO(init_mm.write_protect_seq),
//     MMAP_LOCK_INITIALIZER(init_mm).page_table_lock =
//         __SPIN_LOCK_UNLOCKED(init_mm.page_table_lock),
//     .arg_lock = __SPIN_LOCK_UNLOCKED(init_mm.arg_lock),
//     .mmlist = LIST_HEAD_INIT(init_mm.mmlist),
//     .user_ns = &init_user_ns,
//     .cpu_bitmap = CPU_BITS_NONE,
// #ifdef CONFIG_IOMMU_SVA
//     .pasid = INVALID_IOASID,
// #endif
//     INIT_MM_CONTEXT(init_mm)};

// void setup_initial_init_mm(void *start_code, void *end_code,
// 			   void *end_data, void *brk)
// {
// 	init_mm.start_code = (unsigned long)start_code;
// 	init_mm.end_code = (unsigned long)end_code;
// 	init_mm.end_data = (unsigned long)end_data;
// 	init_mm.brk = (unsigned long)brk;
// }
