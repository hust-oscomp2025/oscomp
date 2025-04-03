#include <kernel/sched.h>
#include <kernel/vfs.h>
#include <kernel/syscall/syscall.h>
#include <kernel/mmu.h>
#include <kernel/util.h>
#include <kernel/time.h>



int64 sys_time(time_t __user* tloc) {
    time_t ktime;
    int64 ret = do_time(&ktime);  // 假设 do_time 填充内核时间到 ktime

    if (ret < 0) {
        return ret;  // 错误直接返回
    }

    // 如果用户传递的 tloc 非空，需要将 ktime 写回用户空间
    if (tloc) {
        if (copy_to_user(tloc, &ktime, sizeof(time_t)) != 0) {
            return -EFAULT;  // 用户指针无效或不可写
        }
    }

    return ktime;  // 返回时间值（兼容 tloc==NULL 的场景）
}
