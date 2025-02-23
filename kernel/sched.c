/*
 * implementing the scheduler
 */

#include "sched.h"
#include "global.h"
#include "spike_interface/spike_utils.h"



//
// insert a process, proc, into the END of ready queue.
//
void insert_to_ready_queue( process* proc ) {
  sprint( "going to insert process %d to ready queue.\n", proc->pid );
  // if the queue is empty in the beginning
  if( ready_queue == NULL ){
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
//extern process procs[NPROC];
void schedule() {
  int hartid = read_tp();

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

  current[hartid] = ready_queue;
  assert( current[hartid]->status == READY );
  ready_queue = ready_queue->queue_next;

  current[hartid]->status = RUNNING;
  sprint( "going to schedule process %d to run.\n", current[hartid]->pid );
  switch_to( current[hartid] );
}
