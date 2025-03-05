

#
# M-mode trap entry point
#
.globl mtrapvec
.align 4
mtrapvec:
    # mscratch -> g_itrframe (cf. kernel/machine/minit.c line 94)
    # swap a0 and mscratch, so that a0 points to interrupt frame,
    # i.e., [a0] = &g_itrframe
    csrrw a0, mscratch, a0

    # save the registers in g_itrframe
    addi t6, a0, 0
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
    # store_all_registers
    # save the original content of a0 in g_itrframe
    csrr t0, mscratch
    sd t0, 72(a0)

    # switch stack (to use stack0) for the rest of machine mode
    # trap handling.
    la sp, stack0
    li a3, 4096
    csrr a4, mhartid
    addi a4, a4, 1
    mul a3, a3, a4
    add sp, sp, a3

    # pointing mscratch back to g_itrframe
    csrw mscratch, a0

    # call machine mode trap handling function
    call handle_mtrap

    # restore all registers, come back to the status before entering
    # machine mode handling.
    csrr t6, mscratch
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

    mret
