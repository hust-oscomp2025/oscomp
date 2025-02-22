#include "global.h"
#include "process.h"
#include "vmm.h"

// process pool. added @lab3_1
process procs[NPROC];

// current points to the currently running user-mode application.
process* current[NCPU];


//内核堆的虚拟头节点
heap_block kernel_heap_head;
