#include "kernel/riscv.h"
#include "kernel/process.h"
#include <spike_interface/spike_utils.h>
#include "spike_interface/spike_file.h"

// #include <util/string.h>
#include <util/string.h>



// added @lab1_3
static void handle_timer() {
  int cpuid = read_csr(mhartid);
  // setup the timer fired at next time (TIMER_INTERVAL from now)
  *(uint64 *)CLINT_MTIMECMP(cpuid) = *(uint64 *)CLINT_MTIMECMP(cpuid) + TIMER_INTERVAL;

  // setup a soft interrupt in sip (S-mode Interrupt Pending) to be handled in S-mode
  write_csr(sip, SIP_SSIP);
}

//
// handle_mtrap calls a handling function according to the type of a machine mode interrupt (trap).
//
void handle_mtrap()
{
	uint64 mcause = read_csr(mcause);
	sprint("machine trap(): unexpected mcause %p\n", mcause);
	sprint("sepc=%lx,mepc=%lx mtval=%p\n", read_csr(sepc),read_csr(mepc), read_csr(mtval));
	panic("unexpected exception happened in M-mode.\n");

}
