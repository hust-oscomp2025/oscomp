#include "kernel/riscv.h"
#include "kernel/process.h"
#include "spike_interface/spike_utils.h"
#include "spike_interface/spike_file.h"

// #include "util/string.h"
#include <string.h>

void error_printer()
{
  int hartid = read_tp();
  uint64 exception_addr = read_csr(mepc);

  addr_line *line_list = current[hartid]->line;
  code_file *file_list = current[hartid]->file;
  char **dir_list = current[hartid]->dir;
  int line_count = current[hartid]->line_count;

  for (int i = 0; i < current[hartid]->line_count; i++)
  {
    if (exception_addr < line_list[i].addr)
    { // illegal instruction is on line (i-1)
      addr_line *excpline = line_list + i - 1;
      char file_path[256];
      char file_contents[8192];
      struct stat file_stat;

      int dir_len = strlen(dir_list[file_list[excpline->file].dir]);
      //'**dir' stores dir string, process->line->file points out the index of code_file
      strcpy(file_path, dir_list[file_list[excpline->file].dir]);
      file_path[dir_len] = '/';
      strcpy(file_path + dir_len + 1, file_list[excpline->file].file);
      // filename places after dir/, code_file->file stores the filename
      // sprint(file_path);
      // sprint("%d",excpline->line);
      // sprint("\n");

      // read illegal instruction through spike_file functions
      spike_file_t *_file_ = spike_file_open(file_path, O_RDONLY, 0);
      spike_file_stat(_file_, &file_stat);
      spike_file_read(_file_, file_contents, file_stat.st_size);
      spike_file_close(_file_);
      int offset = 0, count = 0;
      while (offset < file_stat.st_size)
      {
        int temp = offset;
        while (temp < file_stat.st_size && file_contents[temp] != '\n')
          temp++; // find every line
        if (count == excpline->line - 1)
        {
          file_contents[temp] = '\0';
          sprint("Runtime error at %s:%d\n%s\n", file_path, excpline->line, file_contents + offset);
          break;
        }
        else
        {
          count++;
          offset = temp + 1;
        }
      }
      break;
    }
  }
}

static void handle_instruction_access_fault()
{
  error_printer();
  panic("Instruction access fault!");
}

static void handle_load_access_fault()
{
  error_printer();
  panic("Load access fault!");
}

static void handle_store_access_fault()
{
  error_printer();
  panic("Store/AMO access fault!");
}

static void handle_illegal_instruction()
{
  error_printer();
  panic("Illegal instruction!");
}

static void handle_misaligned_load()
{
  error_printer();
  panic("Misaligned Load!");
}

static void handle_misaligned_store()
{
  error_printer();
  panic("Misaligned AMO!");
}

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
  switch (mcause)
  {
  case CAUSE_MTIMER:
    handle_timer();
    break;
  case CAUSE_FETCH_ACCESS:
    handle_instruction_access_fault();
    break;
  case CAUSE_LOAD_ACCESS:
    handle_load_access_fault();
  case CAUSE_STORE_ACCESS:
    handle_store_access_fault();
    break;
  case CAUSE_ILLEGAL_INSTRUCTION:
    handle_illegal_instruction();
    // TODO (lab1_2): call handle_illegal_instruction to implement illegal instruction
    // interception, and finish lab1_2.
    // panic( "call handle_illegal_instruction to accomplish illegal instruction interception for lab1_2.\n" );

    break;
  case CAUSE_MISALIGNED_LOAD:
    handle_misaligned_load();
    break;
  case CAUSE_MISALIGNED_STORE:
    handle_misaligned_store();
    break;

  default:
    sprint("machine trap(): unexpected m %p\n", mcause);
    sprint("            mepc=%p mtval=%p\n", read_csr(mepc), read_csr(mtval));
    panic("unexpected exception happened in M-mode.\n");
    break;
  }
}
