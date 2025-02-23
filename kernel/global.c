#include "global.h"
#include "process.h"
#include "vmm.h"
#include "semaphore.h"

// process pool. added @lab3_1
process procs[NPROC];

// current points to the currently running user-mode application.
process* current[NCPU];
// 进程就绪队列和阻塞队列
process* ready_queue = NULL;
//process* blocked_queue = NULL;
// 阻塞队列都在信号量里边。


//内核堆的虚拟头节点
heap_block kernel_heap_head;

//信号灯库
semaphore sem_pool[NSEM];