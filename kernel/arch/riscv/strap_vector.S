.section trapsec
.globl trap_sec_start
trap_sec_start:



#
# When a trap (e.g., a syscall from User mode in this lab) happens and the computer
# enters the Supervisor mode, the computer will continue to execute the following
# function (smode_trap_vector) to actually handle the trap.
#
# NOTE: sscratch points to the trapframe of current process before entering
# smode_trap_vector. It is done by reture_to_user function (defined below) when
# scheduling a user-mode application to run.
#
.globl smode_trap_vector
.align 4
smode_trap_vector:
		# sscratch临时保存了用户中断上下文在内核中的位置。
    # swap a0 and sscratch, so that points a0 to the trapframe of current process
    csrrw a0, sscratch, a0

		# t0-t6都是临时寄存器，操作系统或应用程序都不需要保护其中的内容。
    # 这个t6寄存器在用户程序中用不上，所以可以用来做汇编的临时变量，作为保存所有寄存器的内存地址。
    addi t6, a0 , 0

    # store_all_registers is a macro defined in util/load_store.S, it stores contents
    # of all general purpose registers into a piece of memory started from [t6].
    sd ra, 0(t6)
    sd sp, 8(t6)
    sd gp, 16(t6)
    sd tp, 24(t6)
    sd t0, 32(t6)
    sd t1, 40(t6)
    sd t2, 48(t6)
    sd s0, 56(t6)
    sd s1, 64(t6)
    sd a0, 72(t6)
    sd a1, 80(t6)
    sd a2, 88(t6)
    sd a3, 96(t6)
    sd a4, 104(t6)
    sd a5, 112(t6)
    sd a6, 120(t6)
    sd a7, 128(t6)
    sd s2, 136(t6)
    sd s3, 144(t6)
    sd s4, 152(t6)
    sd s5, 160(t6)
    sd s6, 168(t6)
    sd s7, 176(t6)
    sd s8, 184(t6)
    sd s9, 192(t6)
    sd s10, 200(t6)
    sd s11, 208(t6)
    sd t3, 216(t6)
    sd t4, 224(t6)
    sd t5, 232(t6)
    sd t6, 240(t6)

    # come back to save a0 register before entering trap handling in trapframe
    # [t0]=[sscratch]
    csrr t0, sscratch
    sd t0, 72(a0)

    # use the "user kernel" stack (whose pointer stored in p->trapframe->kernel_sp)
    ld sp, 248(a0)

    # load the address of smode_trap_handler() from p->trapframe->kernel_trap
    ld t0, 256(a0)

    # restore kernel page table from p->trapframe->kernel_satp. added @lab2_1
    ld t1, 272(a0)
    csrw satp, t1
    sfence.vma zero, zero

    # jump to smode_trap_handler() that is defined in kernel/trap.c
    jr t0

#
# return from Supervisor mode to User mode, transition is made by using a trapframe,
# which stores the context of a user application.
# return_to_user() takes one parameter, i.e., the pointer (a0 register) pointing to a
# trapframe (defined in kernel/process.h) of the process.
#
.globl return_to_user
return_to_user:
    # a0: TRAPFRAME
    # a1: user page table, for satp.

    # switch to the user page table. added @lab2_1
    csrw satp, a1
    sfence.vma zero, zero

    # [sscratch]=[a0], save a0 in sscratch, so sscratch points to a trapframe now.
    csrw sscratch, a0

    # let [t6]=[a0]
    addi t6, a0, 0

    # restore_all_registers is a assembly macro defined in util/load_store.S.
    # the macro restores all registers from trapframe started from [t6] to all general
    # purpose registers, so as to resort the execution of a process.
    ld ra, 0(t6)
    ld sp, 8(t6)
    ld gp, 16(t6)
    ld tp, 24(t6)
    ld t0, 32(t6)
    ld t1, 40(t6)
    ld t2, 48(t6)
    ld s0, 56(t6)
    ld s1, 64(t6)
    ld a0, 72(t6)
    ld a1, 80(t6)
    ld a2, 88(t6)
    ld a3, 96(t6)
    ld a4, 104(t6)
    ld a5, 112(t6)
    ld a6, 120(t6)
    ld a7, 128(t6)
    ld s2, 136(t6)
    ld s3, 144(t6)
    ld s4, 152(t6)
    ld s5, 160(t6)
    ld s6, 168(t6)
    ld s7, 176(t6)
    ld s8, 184(t6)
    ld s9, 192(t6)
    ld s10, 200(t6)
    ld s11, 208(t6)
    ld t3, 216(t6)
    ld t4, 224(t6)
    ld t5, 232(t6)
    ld t6, 240(t6) 

    # return to user mode and user pc.
    sret
