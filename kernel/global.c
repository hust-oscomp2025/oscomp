#include "global.h"
#include "process.h"

// process pool. added @lab3_1
process procs[NPROC];

// current points to the currently running user-mode application.
process* current[NCPU];