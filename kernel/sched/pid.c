#include <kernel/sched/pid.h>
#include <util/string.h>
#include <spike_interface/spike_utils.h>

struct pid_manager manager;

// 初始化PID管理器
void pid_init() {
  spinlock_init(&manager.lock);

  manager.next_pid = 1;
  memset(manager.pid_map, 0, sizeof(manager.pid_map));
}

// PID分配核心逻辑
pid_t pid_alloc(void) {

  long irq_flags = spinlock_lock_irqsave(&manager.lock);

  for (int i = 0; i < PID_MAX; i++) {
    int32_t current_pid = (manager.next_pid + i) % PID_MAX;
    if (current_pid == 0)
      continue; // 跳过保留PID 0

    int byte_idx = current_pid / 8;
    int bit_idx = current_pid % 8;

    if (!(manager.pid_map[byte_idx] & (1 << bit_idx))) {
      manager.pid_map[byte_idx] |= (1 << bit_idx);
      manager.next_pid = (current_pid + 1) % PID_MAX;
      spinlock_unlock_irqrestore(&manager.lock, irq_flags); // 解锁
      return current_pid;                                   // 成功
    }
  }

  spinlock_unlock_irqrestore(&manager.lock, irq_flags);
  return (-EAGAIN); // 资源耗尽
}

// PID释放
void pid_free(pid_t pid) {
  if (pid <= 0 || pid >= PID_MAX)
    panic("pid_free: wrong pid\n");

  long irq_flags = spinlock_lock_irqsave(&manager.lock);

  int byte_idx = pid / 8;
  int bit_idx = pid % 8;
  manager.pid_map[byte_idx] &= ~(1 << bit_idx);

  spinlock_unlock_irqrestore(&manager.lock, irq_flags);
}