#include <kernel/sched.h>
#include <kernel/types.h>
#include <kernel/util.h>
#include <kernel/util/print.h>
#include <kernel/mmu.h>

/* Global variable to track the signal that caused an interrupt */
int32 g_signal_pending = 0;

/* Sigset operations */

/**
 * sigemptyset - Initialize and empty a signal set
 * @set: Signal set to initialize
 *
 * Returns 0 on success, negative error code on failure
 */
int32 sigemptyset(sigset_t *set)
{
    if (!set)
        return -EINVAL;
        
    memset(set, 0, sizeof(sigset_t));
    return 0;
}

/**
 * sigfillset - Initialize and fill a signal set
 * @set: Signal set to initialize
 *
 * Returns 0 on success, negative error code on failure
 */
int32 sigfillset(sigset_t *set)
{
    if (!set)
        return -EINVAL;
        
    memset(set, 0xFF, sizeof(sigset_t));
    return 0;
}

/**
 * sigaddset - Add a signal to a signal set
 * @set: Signal set to modify
 * @signo: Signal number to add
 *
 * Returns 0 on success, negative error code on failure
 */
int32 sigaddset(sigset_t *set, int32 signo)
{
    if (!set || signo <= 0 || signo >= NSIG)
        return -EINVAL;
        
    set->__val[(signo - 1) / (8 * sizeof(uint64))] |= 
        1UL << ((signo - 1) % (8 * sizeof(uint64)));
    return 0;
}

/**
 * sigdelset - Remove a signal from a signal set
 * @set: Signal set to modify
 * @signo: Signal number to remove
 *
 * Returns 0 on success, negative error code on failure
 */
int32 sigdelset(sigset_t *set, int32 signo)
{
    if (!set || signo <= 0 || signo >= NSIG)
        return -EINVAL;
        
    set->__val[(signo - 1) / (8 * sizeof(uint64))] &= 
        ~(1UL << ((signo - 1) % (8 * sizeof(uint64))));
    return 0;
}

/**
 * sigismember - Test if a signal is in a signal set
 * @set: Signal set to test
 * @signo: Signal number to test for
 *
 * Returns 1 if the signal is in the set, 0 if not, negative error code on failure
 */
int32 sigismember(const sigset_t *set, int32 signo)
{
    if (!set || signo <= 0 || signo >= NSIG)
        return -EINVAL;
        
    return !!(set->__val[(signo - 1) / (8 * sizeof(uint64))] & 
        (1UL << ((signo - 1) % (8 * sizeof(uint64)))));
}

/**
 * do_send_signal - Core signal sending function
 * @pid: Process ID to send signal to
 * @sig: Signal to send
 *
 * Returns 0 on success, negative error code on failure
 */
int32 do_send_signal(pid_t pid, int32 sig)
{
    struct task_struct *p;
    
    if (sig <= 0 || sig >= NSIG)
        return -EINVAL;

    // Special case: pid 0 means current process
    if (pid == 0) {
        p = current_task();
        if (!p)
            return -ESRCH;
    } else {
        p = find_process_by_pid(pid);
        if (!p)
            return -ESRCH;
    }
    
    // Handle SIGKILL and SIGSTOP specially
    if (sig == SIGKILL || sig == SIGSTOP) {
        // These signals cannot be blocked or ignored
        
        // For SIGKILL, terminate the process immediately
        if (sig == SIGKILL) {
            kprintf("Process %d killed by signal %d\n", p->pid, sig);
            // Implement process termination logic
            // do_exit(p, sig);
            return 0;
        }
        
        // For SIGSTOP, stop the process
        if (sig == SIGSTOP) {
            kprintf("Process %d stopped by signal %d\n", p->pid, sig);
            // Implement process stop logic
            // p->state = TASK_STOPPED;
            return 0;
        }
    }
    
    // Add signal to pending set
    sigaddset(&p->pending, sig);
    
    // Wake up if sleeping
    // if (p->state == TASK_INTERRUPTIBLE)
    //     wake_up_process(p);
    
    return 0;
}

/**
 * do_sigaction - Set or get signal handler
 * @sig: Signal number
 * @act: New action (NULL to just get the current action)
 * @oldact: Where to store the old action (NULL if not needed)
 *
 * Returns 0 on success, negative error code on failure
 */
int32 do_sigaction(int32 sig, const struct sigaction *act, struct sigaction *oldact)
{
    struct task_struct *p = current_task();
    
    if (!p)
        return -ESRCH;
        
    if (sig <= 0 || sig >= NSIG)
        return -EINVAL;
        
    // Cannot change action for SIGKILL and SIGSTOP
    if (sig == SIGKILL || sig == SIGSTOP)
        return -EINVAL;
    
    // Store old action if requested
    if (oldact)
        *oldact = p->sighand[sig - 1];
    
    // Set new action if provided
    if (act)
        p->sighand[sig - 1] = *act;
    
    return 0;
}

/**
 * do_sigprocmask - Change the signal mask
 * @how: How to modify the mask (SIG_BLOCK, SIG_UNBLOCK, SIG_SETMASK)
 * @set: Signal set to apply
 * @oldset: Where to store the old mask (NULL if not needed)
 *
 * Returns 0 on success, negative error code on failure
 */
int32 do_sigprocmask(int32 how, const sigset_t *set, sigset_t *oldset)
{
    struct task_struct *p = current_task();
    sigset_t new_blocked;
    
    if (!p)
        return -ESRCH;
    
    // Store old mask if requested
    if (oldset)
        *oldset = p->blocked;
    
    // Return if no change requested
    if (!set)
        return 0;
    
    // Apply new mask according to 'how'
    switch (how) {
        case SIG_BLOCK:
            // Block signals in set
            new_blocked = p->blocked;
            for (int32 i = 0; i < _SIGSET_NWORDS; i++)
                new_blocked.__val[i] |= set->__val[i];
            break;
            
        case SIG_UNBLOCK:
            // Unblock signals in set
            new_blocked = p->blocked;
            for (int32 i = 0; i < _SIGSET_NWORDS; i++)
                new_blocked.__val[i] &= ~set->__val[i];
            break;
            
        case SIG_SETMASK:
            // Set mask to the provided set
            new_blocked = *set;
            break;
            
        default:
            return -EINVAL;
    }
    
    // SIGKILL and SIGSTOP cannot be blocked
    sigdelset(&new_blocked, SIGKILL);
    sigdelset(&new_blocked, SIGSTOP);
    
    // Apply the new mask
    p->blocked = new_blocked;
    
    return 0;
}

/**
 * do_kill - Send a signal to a process
 * @pid: Process ID to target
 * @sig: Signal to send
 *
 * Returns 0 on success, negative error code on failure
 */
int32 do_kill(pid_t pid, int32 sig)
{
    // Special handling for various pid values:
    if (pid > 0) {
        // Send to specific process
        return do_send_signal(pid, sig);
    }
    else if (pid == 0) {
        // Send to all processes in the same process group
        // For simplicity in this implementation, we'll just send to current process
        return do_send_signal(0, sig);
    }
    else if (pid == -1) {
        // Send to all processes for which the calling process has permission to send
        // For simplicity, we'll just send to current process
        return do_send_signal(0, sig);
    }
    else {
        // Send to all processes in the process group -pid
        // For simplicity, we'll just send to current process
        return do_send_signal(0, sig);
    }
}

/**
 * do_signal_delivery - Check and handle any pending signals
 *
 * This should be called when returning to user mode
 */
void do_signal_delivery(void)
{
    struct task_struct *p = current_task();
    int32 sig;
    
    if (!p)
        return;
    
    // Find the first pending and unblocked signal
    for (sig = 1; sig < NSIG; sig++) {
        if (sigismember(&p->pending, sig) && !sigismember(&p->blocked, sig)) {
            // Found a signal to deliver
            sigdelset(&p->pending, sig);
            
            // Check the signal handler action
            if (p->sighand[sig - 1].sa_handler == SIG_IGN) {
                // Signal is ignored, just continue
                continue;
            } 
            else if (p->sighand[sig - 1].sa_handler == SIG_DFL) {
                // Default action
                switch (sig) {
                    case SIGTERM:
                    case SIGKILL:
                    case SIGSEGV:
                    case SIGBUS:
                    case SIGILL:
                    case SIGFPE:
                    case SIGQUIT:
                    case SIGABRT:
                        // Default is to terminate the process
                        kprintf("Process %d terminated by signal %d\n", p->pid, sig);
                        // Implement termination here
                        // do_exit(p, sig);
                        break;
                        
                    case SIGSTOP:
                    case SIGTSTP:
                    case SIGTTIN:
                    case SIGTTOU:
                        // Default is to stop the process
                        kprintf("Process %d stopped by signal %d\n", p->pid, sig);
                        // Implement stop logic here
                        // p->state = TASK_STOPPED;
                        break;
                        
                    case SIGCONT:
                        // Default is to continue if stopped
                        // if (p->state == TASK_STOPPED)
                        //    p->state = TASK_RUNNING;
                        break;
                        
                    default:
                        // Default for other signals is to ignore
                        break;
                }
            } 
            else {
                // Custom handler - set up the stack frame and jump to the handler
                kprintf("Process %d: delivering signal %d to handler\n", p->pid, sig);
                // This needs architecture-specific code to set up a signal frame
                // setup_signal_frame(p, sig);
                break;
            }
        }
    }
}

/**
 * do_sigsuspend - Temporarily replace signal mask and suspend
 * @mask: Signal mask to use while suspended
 *
 * Returns -EINTR when a signal is caught and handled
 */
int32 do_sigsuspend(const sigset_t *mask)
{
    struct task_struct *p = current_task();
    sigset_t old_blocked;
    
    if (!p || !mask)
        return -EINVAL;
    
    // Save the current signal mask
    old_blocked = p->blocked;
    
    // Set the new mask
    p->blocked = *mask;
    
    // SIGKILL and SIGSTOP cannot be blocked
    sigdelset(&p->blocked, SIGKILL);
    sigdelset(&p->blocked, SIGSTOP);
    
    // Now wait for a signal
    // set_current_state(TASK_INTERRUPTIBLE);
    // schedule();
    
    // Restore the original mask
    p->blocked = old_blocked;
    
    // Always return -EINTR
    return -EINTR;
}

/* System call handlers for signal functions */

/**
 * sys_kill - System call handler for kill()
 * @pid: Process ID to send signal to
 * @sig: Signal to send
 */
int64 sys_kill(pid_t pid, int32 sig)
{
    return do_kill(pid, sig);
}

/**
 * sys_sigaction - System call handler for sigaction()
 * @sig: Signal number
 * @act_user: User space pointer to new action
 * @oldact_user: User space pointer to store old action
 */
int64 sys_sigaction(int32 sig, const struct sigaction *act_user, struct sigaction *oldact_user)
{
    struct sigaction kernel_act, kernel_oldact;
    int32 ret;
    
    if (act_user) {
        // Copy action from user space
        if (copy_from_user(&kernel_act, act_user, sizeof(struct sigaction)))
            return -EFAULT;
    }
    
    ret = do_sigaction(sig, act_user ? &kernel_act : NULL, oldact_user ? &kernel_oldact : NULL);
    
    if (ret == 0 && oldact_user) {
        // Copy old action back to user space
        if (copy_to_user(oldact_user, &kernel_oldact, sizeof(struct sigaction)))
            return -EFAULT;
    }
    
    return ret;
}

/**
 * sys_sigprocmask - System call handler for sigprocmask()
 * @how: How to modify the mask
 * @set_user: User space pointer to new signal set
 * @oldset_user: User space pointer to store old signal set
 */
int64 sys_sigprocmask(int32 how, const sigset_t *set_user, sigset_t *oldset_user)
{
    sigset_t kernel_set, kernel_oldset;
    int32 ret;
    
    if (set_user) {
        // Copy set from user space
        if (copy_from_user(&kernel_set, set_user, sizeof(sigset_t)))
            return -EFAULT;
    }
    
    ret = do_sigprocmask(how, set_user ? &kernel_set : NULL, oldset_user ? &kernel_oldset : NULL);
    
    if (ret == 0 && oldset_user) {
        // Copy old set back to user space
        if (copy_to_user(oldset_user, &kernel_oldset, sizeof(sigset_t)))
            return -EFAULT;
    }
    
    return ret;
}

/**
 * sys_sigsuspend - System call handler for sigsuspend()
 * @mask_user: User space pointer to signal mask
 */
int64 sys_sigsuspend(const sigset_t *mask_user)
{
    sigset_t kernel_mask;
    
    if (!mask_user)
        return -EFAULT;
    
    // Copy mask from user space
    if (copy_from_user(&kernel_mask, mask_user, sizeof(sigset_t)))
        return -EFAULT;
    
    return do_sigsuspend(&kernel_mask);
}