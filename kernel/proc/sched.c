/*
 * implementing the scheduler
 */

#include <kernel/sched.h>
#include <kernel/kmalloc.h>
#include <spike_interface/spike_utils.h>


#define store_all_registers(t6) \
    asm volatile(                \
        "sd ra, 0(%0)\n"          \
        "sd sp, 8(%0)\n"          \
        "sd gp, 16(%0)\n"         \
        "sd tp, 24(%0)\n"         \
        "sd t0, 32(%0)\n"         \
        "sd t1, 40(%0)\n"         \
        "sd t2, 48(%0)\n"         \
        "sd s0, 56(%0)\n"         \
        "sd s1, 64(%0)\n"         \
        "sd a0, 72(%0)\n"         \
        "sd a1, 80(%0)\n"         \
        "sd a2, 88(%0)\n"         \
        "sd a3, 96(%0)\n"         \
        "sd a4, 104(%0)\n"        \
        "sd a5, 112(%0)\n"        \
        "sd a6, 120(%0)\n"        \
        "sd a7, 128(%0)\n"        \
        "sd s2, 136(%0)\n"        \
        "sd s3, 144(%0)\n"        \
        "sd s4, 152(%0)\n"        \
        "sd s5, 160(%0)\n"        \
        "sd s6, 168(%0)\n"        \
        "sd s7, 176(%0)\n"        \
        "sd s8, 184(%0)\n"        \
        "sd s9, 192(%0)\n"        \
        "sd s10, 200(%0)\n"       \
        "sd s11, 208(%0)\n"       \
        "sd t3, 216(%0)\n"        \
        "sd t4, 224(%0)\n"        \
        "sd t5, 232(%0)\n"        \
        "sd t6, 240(%0)\n"        \
        :                         \
        : "r"(t6)                 \
        : "memory"                \
    )
#define restore_all_registers(t6) \
    asm volatile(                \
        "ld ra, 0(%0)\n"          \
        "ld sp, 8(%0)\n"          \
        "ld gp, 16(%0)\n"         \
        "ld tp, 24(%0)\n"         \
        "ld t0, 32(%0)\n"         \
        "ld t1, 40(%0)\n"         \
        "ld t2, 48(%0)\n"         \
        "ld s0, 56(%0)\n"         \
        "ld s1, 64(%0)\n"         \
        "ld a0, 72(%0)\n"         \
        "ld a1, 80(%0)\n"         \
        "ld a2, 88(%0)\n"         \
        "ld a3, 96(%0)\n"         \
        "ld a4, 104(%0)\n"        \
        "ld a5, 112(%0)\n"        \
        "ld a6, 120(%0)\n"        \
        "ld a7, 128(%0)\n"        \
        "ld s2, 136(%0)\n"        \
        "ld s3, 144(%0)\n"        \
        "ld s4, 152(%0)\n"        \
        "ld s5, 160(%0)\n"        \
        "ld s6, 168(%0)\n"        \
        "ld s7, 176(%0)\n"        \
        "ld s8, 184(%0)\n"        \
        "ld s9, 192(%0)\n"        \
        "ld s10, 200(%0)\n"       \
        "ld s11, 208(%0)\n"       \
        "ld t3, 216(%0)\n"        \
        "ld t4, 224(%0)\n"        \
        "ld t5, 232(%0)\n"        \
        "ld t6, 240(%0)\n"        \
        :                         \
        : "r"(t6)                 \
        : "memory"                \
    )


process* ready_queue = NULL;
//
// insert a process, proc, into the END of ready queue.
//
void insert_to_ready_queue( process* proc ) {
  sprint( "going to insert process %d to ready queue.\n", proc->pid );
  // if the queue is empty in the beginning
  if( ready_queue == NULL ){
		//sprint("ready_queue is empty\n");
    proc->status = READY;
    proc->queue_next = NULL;
    ready_queue = proc;
    return;
  }
  // ready queue is not empty
  process *p;
  // browse the ready queue to see if proc is already in-queue
  for( p=ready_queue; p->queue_next!=NULL; p=p->queue_next )
    if( p == proc ) return;  //already in queue
  // p points to the last element of the ready queue
  if( p==proc ) return;
  p->queue_next = proc;
  proc->status = READY;
  proc->queue_next = NULL;
  return;
}

//
// choose a proc from the ready queue, and put it to run.
// note: schedule() does not take care of previous current process. If the current
// process is still runnable, you should place it into the ready queue (by calling
// ready_queue_insert), and then call schedule().
//
void schedule() {
	extern process procs[NPROC];
	int hartid = read_tp();
	process* cur = CURRENT;
			//sprint("debug\n");
	if(cur && cur->status == BLOCKED && cur->ktrapframe == NULL){
		cur->ktrapframe = (trapframe *)kmalloc(sizeof(trapframe));
		store_all_registers(cur->ktrapframe);
		//sprint("cur->ktrapframe->regs.ra=0x%x\n",cur->ktrapframe->regs.ra);
	}

  if ( !ready_queue ){
    // by default, if there are no ready process, and all processes are in the status of
    // FREE and ZOMBIE, we should shutdown the emulated RISC-V machine.
    int should_shutdown = 1;

    for( int i=0; i<NPROC; i++ )
      if( (procs[i].status != FREE) && (procs[i].status != ZOMBIE) ){
        should_shutdown = 0;
        sprint( "ready queue empty, but process %d is not in free/zombie state:%d\n", 
          i, procs[i].status );
				
      }

    // 如果所有进程都在 FREE 或 ZOMBIE 状态，允许关机
    if( should_shutdown ) {
      // 确保只有 hartid == 0 的核心执行 shutdown
      if (hartid == 0) {
        sprint( "no more ready processes, system shutdown now.\n" );
        shutdown( 0 );  // 只有核心 0 执行关机
      }
      // 否则，其他核心等待关机标志
      else {
        while (1) {
          // 自旋等待，直到 hartid == 0 核心完成 shutdown
        }
      }
    } else {
      panic( "Not handled: we should let system wait for unfinished processes.\n" );
    }
  }
	


  cur = ready_queue;
	assert( cur->status == READY );
	ready_queue = ready_queue->queue_next;
	CURRENT = cur;
	cur->status = RUNNING;
	if(cur->ktrapframe != NULL){
		restore_all_registers(cur->ktrapframe);
		kfree(cur->ktrapframe);
		cur->ktrapframe = NULL;
		return;
	}
  

  sprint( "going to schedule process %d to run.\n", cur->pid );
  switch_to( cur );
}
