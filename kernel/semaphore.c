#include <kernel/semaphore.h>



#include <kernel/sched/sched.h>
#include <kernel/util/print.h>
//信号灯库
semaphore sem_pool[NSEM];
// 获取一个新的信号灯编号，成功返回0-64，失败返回-1
int32 sem_new(int32 initial_value) {
  for (int32 i = 0; i < NSEM; i++) {
    if (sem_pool[i].isActive == 0) {
      sem_pool[i].isActive = 1;
      sem_pool[i].value = initial_value;
			//kprintf("sem %d initialvalue %d\n",i,initial_value);
      // sem_pool[i].pid = pid;
      sem_pool[i].wait_queue = NULL;
      return i;
    }
  }
  return -1;
}

void sem_free(int32 sem_index) {
  if (sem_index < 0 || sem_index >= NSEM || !sem_pool[sem_index].isActive) {
    panic("invalid sem_index!");
  }

  // 在进程结束以后释放这个进程申请的信号量（待实现）
  return;
}

int32 sem_P(int32 sem_index) {
  // //kprintf("Received calling sem_P index = %d\n", sem_index);
  // if (sem_index < 0 || sem_index >= NSEM || !sem_pool[sem_index].isActive) {
  //   panic("invalid sem_index!");
  // }

  // semaphore *sem = &sem_pool[sem_index];
  // if (sem->value > 0) {
  //   return sem->value--;
  // } else {
  //   // kprintf("BLOCK!\n");
  //   int32 hartid = read_tp();
  //   struct task_struct *cur = CURRENT;
  //   cur->state = TASK_INTERRUPTIBLE;
  //   cur->ready_queue_node = sem->wait_queue;
  //   sem->wait_queue = cur;

  //   schedule();

  //   // kprintf("return from blocking!\n");
  //   return 0;
  //   // 直接嵌入保存内核上下文到process->ktrapframe的汇编代码
  // }
	return 0;
}

// 我们这里为了实现简单，直接把信号队列当成信号栈使了。
// 如果说有进程等待信号量，返回0，不然返回增加后的信号资源量sem->value
int32 sem_V(int32 sem_index) {
  // if (sem_index < 0 || sem_index >= NSEM || !sem_pool[sem_index].isActive) {
  //   panic("invalid sem_index!");
  // }
  // semaphore *sem = &sem_pool[sem_index];

  // if (sem->value == 0 && sem->wait_queue != NULL) {
  //   insert_to_ready_queue(sem->wait_queue);
	// 	//insert_to_ready_queue(current);
  //   // sem->wait_queue->state = READY;
  //   sem->wait_queue = sem->wait_queue->queue_next;
  //   //schedule();
  //   return 0;
  // }
	// return sem->value++;
	return 0;
}