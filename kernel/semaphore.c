#include "semaphore.h"

#include "global.h"
#include "sched.h"
#include "spike_interface/spike_utils.h"

// 获取一个新的信号灯编号，成功返回0-64，失败返回-1
int sem_new(int initial_value, int pid) {
  for (int i = 0; i < NSEM; i++) {
    if (sem_pool[i].isActive == 0) {
			sem_pool[i].isActive = 1;
      sem_pool[i].value = initial_value;
      sem_pool[i].pid = pid;
      return i;
    }
  }
  return -1;
}

void sem_free(int sem_index) {
  if (sem_index < 0 || sem_index >= NSEM || !sem_pool[sem_index].isActive) {
		panic("invalid sem_index!");
  }

  // 在进程结束以后释放这个进程申请的信号量（待实现）
  return;
}

int sem_P(int sem_index) {
  if (sem_index < 0 || sem_index >= NSEM || !sem_pool[sem_index].isActive) {
		panic("invalid sem_index!");
  }

  semaphore *sem = &sem_pool[sem_index];
  if (sem->value > 0) {
    return sem->value--;
  } else {
		//sprint("BLOCK!\n");
    int hartid = read_tp();
    process *cur = current[hartid];
    cur->status = BLOCKED;
    cur->queue_next = sem->wait_queue;
    sem->wait_queue = cur;
    schedule();
		return 0;
  }
}

// 我们这里为了实现简单，直接把信号队列当成信号栈使了。
// 如果说有进程等待信号量，返回0，不然返回增加后的信号资源量sem->value
int sem_V(int sem_index) {
  if (sem_index < 0 || sem_index >= NSEM || !sem_pool[sem_index].isActive) {
    return -1;
  }
  semaphore *sem = &sem_pool[sem_index];
  if (sem->wait_queue != NULL) {
    process *wakeup_process = sem->wait_queue;
    sem->wait_queue = wakeup_process->queue_next;

    wakeup_process->status = READY;
    wakeup_process->queue_next = ready_queue;
    ready_queue = wakeup_process;
    schedule();
    return 0;
  } else {
    sem->value++;
    return sem->value;
  }
}