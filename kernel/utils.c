#include "spike_interface/spike_utils.h"
#include "config.h"
#include "riscv.h"



void Sprint(const char* s, ...) {
    int hartid = read_tp();
    if (NCPU > 1)
        sprint("hartid = %d: ", hartid);
    sprint("%s\n", s);
}