/*
 * routines that scan and load a (host) Executable and Linkable Format (ELF) file
 * into the (emulated) memory.
 */

#include "elf.h"
#include "string.h"
#include "riscv.h"
#include "spike_interface/spike_utils.h"

typedef struct elf_info_t {
  spike_file_t *f;
  process *p;
} elf_info;

//
// the implementation of allocater. allocates memory space for later segment loading
//
static void *elf_alloc_mb(elf_ctx *ctx, uint64 elf_pa, uint64 elf_va, uint64 size) {
  // directly returns the virtual address as we are in the Bare mode in lab1_x
  return (void *)elf_va;
}

//
// actual file reading, using the spike file interface.
//
static uint64 elf_fpread(elf_ctx *ctx, void *dest, uint64 nb, uint64 offset) {
  elf_info *msg = (elf_info *)ctx->info;
  // call spike file utility to load the content of elf file into memory.
  // spike_file_pread will read the elf file (msg->f) from offset to memory (indicated by
  // *dest) for nb bytes.
  return spike_file_pread(msg->f, dest, nb, offset);
}

//
// init elf_ctx, a data structure that loads the elf.
//
elf_status elf_init(elf_ctx *ctx, void *info) {
  ctx->info = info;

  // load the elf header
  if (elf_fpread(ctx, &ctx->ehdr, sizeof(ctx->ehdr), 0) != sizeof(ctx->ehdr)) return EL_EIO;

  // check the signature (magic value) of the elf
  if (ctx->ehdr.magic != ELF_MAGIC) return EL_NOTELF;

  return EL_OK;
}

//
// load the elf segments to memory regions as we are in Bare mode in lab1
//
elf_status elf_load(elf_ctx *ctx) {
  // elf_prog_header structure is defined in kernel/elf.h
  elf_prog_header ph_addr;
  
  int i, off;

  // traverse the elf program segment headers
  //sprint("%x\n", ctx->ehdr.phoff);
  for (i = 0, off = ctx->ehdr.phoff; i < ctx->ehdr.phnum; i++, off += sizeof(ph_addr)) {
    // read segment headers
    if (elf_fpread(ctx, (void *)&ph_addr, sizeof(ph_addr), off) != sizeof(ph_addr)) return EL_EIO;

    if (ph_addr.type != ELF_PROG_LOAD) continue;
    if (ph_addr.memsz < ph_addr.filesz) return EL_ERR;
    if (ph_addr.vaddr + ph_addr.memsz < ph_addr.vaddr) return EL_ERR;

    // allocate memory block before elf loading
    //sprint("%x\n", ph_addr.vaddr);
    void *dest = elf_alloc_mb(ctx, ph_addr.vaddr, ph_addr.vaddr, ph_addr.memsz);

    // actual loading
    if (elf_fpread(ctx, dest, ph_addr.memsz, ph_addr.off) != ph_addr.memsz)
      return EL_EIO;
  }
  /*lab1_challenge1*/
  int ret;
  if((ret = load_function_name(ctx)) != EL_OK) return ret;


  return EL_OK;
}

typedef union {
  uint64 buf[MAX_CMDLINE_ARGS];
  char *argv[MAX_CMDLINE_ARGS];
} arg_buf;

//
// returns the number (should be 1) of string(s) after PKE kernel in command line.
// and store the string(s) in arg_bug_msg.
//
static size_t parse_args(arg_buf *arg_bug_msg) {
  // HTIFSYS_getmainvars frontend call reads command arguments to (input) *arg_bug_msg
  long r = frontend_syscall(HTIFSYS_getmainvars, (uint64)arg_bug_msg,
      sizeof(*arg_bug_msg), 0, 0, 0, 0, 0);
  kassert(r == 0);

  size_t pk_argc = arg_bug_msg->buf[0];
  uint64 *pk_argv = &arg_bug_msg->buf[1];

  int arg = 1;  // skip the PKE OS kernel string, leave behind only the application name
  for (size_t i = 0; arg + i < pk_argc; i++)
    arg_bug_msg->argv[i] = (char *)(uintptr_t)pk_argv[arg + i];

  //returns the number of strings after PKE kernel in command line
  return pk_argc - arg;
}

//
// load the elf of user application, by using the spike file interface.
//
void load_bincode_from_host_elf(process *p) {
  arg_buf arg_bug_msg;

  // retrieve command line arguements
  size_t argc = parse_args(&arg_bug_msg);
  if (!argc) panic("You need to specify the application program!\n");

  sprint("Application: %s\n", arg_bug_msg.argv[0]);

  //elf loading. elf_ctx is defined in kernel/elf.h, used to track the loading process.
  elf_ctx elfloader;
  // elf_info is defined above, used to tie the elf file and its corresponding process.
  elf_info info;

  info.f = spike_file_open(arg_bug_msg.argv[0], O_RDONLY, 0);
  info.p = p;
  // IS_ERR_VALUE is a macro defined in spike_interface/spike_htif.h
  if (IS_ERR_VALUE(info.f)) panic("Fail on openning the input application program.\n");

  // init elfloader context. elf_init() is defined above.
  if (elf_init(&elfloader, &info) != EL_OK) panic("fail to init elfloader.\n");

  // load elf. elf_load() is defined above.
  if (elf_load(&elfloader) != EL_OK) panic("Fail on loading elf.\n");

  // entry (virtual, also physical in lab1_x) address
  p->trapframe->epc = elfloader.ehdr.entry;

  // close the host spike file
  spike_file_close( info.f );

  sprint("Application program entry point (virtual address): 0x%lx\n", p->trapframe->epc);
}

// lab1_challenge1
elf_symbol function_symbols[SYMBOL_NUM];
char function_names[SYMBOL_NUM][SYMBOL_LENGTH];
int function_count;
elf_status load_function_name(elf_ctx *ctx){
  elf_section_header symbol_section_header;
  elf_section_header string_section_header;
  elf_section_header shstr_section_header;
  uint64 shstr_offset;

  shstr_offset = ctx->ehdr.shoff + ctx->ehdr.shstrndx * sizeof(elf_section_header);
  elf_fpread(ctx, (void*)&shstr_section_header, sizeof(shstr_section_header), shstr_offset);

  char tmp_str[256*100];
  elf_fpread(ctx, &tmp_str, shstr_section_header.size, shstr_section_header.offset);

  elf_section_header temp_sh;
  for(int i = 0; i < ctx->ehdr.shnum; i++) {
    elf_fpread(ctx, (void*)&temp_sh, sizeof(temp_sh), ctx->ehdr.shoff+i*ctx->ehdr.shentsize);
    uint32 type = temp_sh.type;
    if(type == ELF_SHT_SYMTAB){
      symbol_section_header = temp_sh;
    } else if(type == ELF_SHT_STRTAB && strcmp(tmp_str+temp_sh.name,".strtab")==0){
      string_section_header = temp_sh;
    }
  }
  int count = 0;
  int symbol_num = symbol_section_header.size / sizeof(elf_symbol);
  for(int i = 0;i < symbol_num; i++){
    elf_symbol symbol;
    elf_fpread(ctx, (void*)&symbol, sizeof(symbol), symbol_section_header.offset + i * sizeof(elf_symbol));
    if(symbol.name == 0) continue;
    if(symbol.info == 18){
      char symbol_name[256];
      elf_fpread(ctx,
                 (void*)symbol_name,
                 sizeof(symbol_name),
                 string_section_header.offset + symbol.name
      );
      function_symbols[count] = symbol;
      strcpy(function_names[count++],symbol_name);
      print_elf_symbol(&symbol,count - 1);
    }
   
  }
  function_count = count; 

  return EL_OK;
}

char* locate_function_name(uint64 epc){
  int find_index = 0;
  uint64 closest_entry = 0x0;
  for(int i = 0;i < function_count;i++){
    uint64 function_entry = function_symbols[i].value;
    if(function_entry < epc && function_entry > closest_entry){
      closest_entry = function_entry;
      find_index = i;
    }
  }
  return function_names[find_index];
}


//debug函数，查看elf符号信息
void print_elf_symbol(const elf_symbol *symbol, int index) {
    if (symbol == NULL) {
        sprint("Invalid symbol\n");
        return;
    }


    // 分解 info 字段为类型和绑定属性
    unsigned char type = symbol->info & 0x0F;  // 低 4 位为类型
    unsigned char binding = symbol->info >> 4; // 高 4 位为绑定

    // 打印符号信息
    sprint("Symbol name: %s\n", function_names[index]);
    sprint("Type:          0x%lx\n", type);
    sprint("Binding:       0x%lx\n", binding);
    sprint("Other:         0x%lx\n", symbol->other);
    sprint("Section Index: 0x%lx\n", symbol->shndx);
    sprint("Value:         0x%lx\n", symbol->value);
    sprint("Size:          0x%lx\n", symbol->size);
}