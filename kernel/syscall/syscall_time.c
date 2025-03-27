#include <kernel/syscall/syscall_ids.h>
#include <kernel/time.h>

/**
 * sys_time - System call handler for time()
 * @timer_user: User space pointer to store the time
 *
 * Returns the current time in seconds since the Unix epoch.
 * If timer_user is not NULL, the time is also stored at the user-provided address.
 */
int64 sys_time(time_t *timer_user)
{
    time_t current;
    
    /* Get the current time */
    current = do_time(NULL);
    
    /* If timer_user is a valid pointer, copy the time to user space */
    if (timer_user) {
        if (copy_to_user(timer_user, &current, sizeof(time_t)))
            return -EFAULT;
    }
    
    return current;
}