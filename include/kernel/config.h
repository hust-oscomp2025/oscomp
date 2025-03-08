#ifndef _CONFIG_H_
#define _CONFIG_H_

// we use two HART (cpu) in challenge3
#define NCPU 1

//interval of timer interrupt. added @lab1_3
#define TIMER_INTERVAL 1000000

// the maximum memory space that PKE is allowed to manage. added @lab2_1
#define PKE_MAX_ALLOWABLE_RAM 128 * 1024 * 1024

// RISC-V machine places its physical memory above DRAM_BASE
#define DRAM_BASE 0x80000000

// the beginning virtual address of PKE kernel
#define KERN_BASE 0x80200000

// virtual address of stack top of user process
#define USER_STACK_TOP 0x80000000

// start virtual address (4MB) of our simple heap. added @lab2_2
#define USER_FREE_ADDRESS_START 0x00000000 + PGSIZE * 1024



// the ending physical address that PKE observes. added @lab2_1
#define PHYS_TOP (DRAM_BASE + PKE_MAX_ALLOWABLE_RAM)

#endif
