/*
 * routines that scan and load a (host) Executable and Linkable Format (ELF)
 * file into the (emulated) memory.
 */

#include "elf.h"
#include "pmm.h"
#include "riscv.h"
#include "spike_interface/spike_utils.h"
#include "string.h"
#include "utils.h"
#include "vmm.h"

typedef struct elf_info_t {
  spike_file_t *f;
  process *p;
} elf_info;

//
// actual file reading, using the spike file interface.
//
static uint64 elf_fpread(elf_ctx *ctx, void *dest, uint64 nb, uint64 offset) {
  elf_info *msg = (elf_info *)ctx->info;
  // call spike file utility to load the content of elf file into memory.
  // spike_file_pread will read the elf file (msg->f) from offset to memory
  // (indicated by *dest) for nb bytes.
  return spike_file_pread(msg->f, dest, nb, offset);
}

static void *elf_alloc_page(elf_ctx *ctx, uint64 elf_va) {
  elf_info *msg = (elf_info *)ctx->info;
  void *pa = Alloc_page();
  user_vm_map((pagetable_t)msg->p->pagetable, elf_va, PGSIZE, (uint64)pa,
              prot_to_type(PROT_WRITE | PROT_READ | PROT_EXEC, 1));
  return pa;
}

elf_status elf_load_segment(elf_ctx *ctx, elf_prog_header *ph_addr) {
  process *target_ps = ((elf_info *)ctx->info)->p;
  size_t num_bytes = ph_addr->memsz;
  uint64 num_pages = (ph_addr->memsz + PGSIZE - 1) / PGSIZE; // 向上取整
  void *first_pa = NULL;
  for (uint64 i = 0; i < num_pages; i++) {
    void *pa = elf_alloc_page(ctx, ph_addr->vaddr + i * PGSIZE);
    if (i == 0)
      first_pa = pa;
    // actual loading
    size_t load_bytes;
    if (i == num_pages - 1) {
      load_bytes = num_bytes % PGSIZE;
    } else {
      load_bytes = PGSIZE;
    }
    if (elf_fpread(ctx, pa, load_bytes, ph_addr->off + i * PGSIZE) !=
        load_bytes)
      return EL_EIO;
  }
  mapped_region *mapped_info_write = NULL;
  for (int j = 0; j < PGSIZE / sizeof(mapped_region); j++) {
    if (target_ps->mapped_info[j].va == 0x0) {
      mapped_info_write = &(target_ps->mapped_info[j]);
      mapped_info_write->va = ph_addr->vaddr;
      mapped_info_write->npages = num_pages;
      if (ph_addr->flags == (SEGMENT_READABLE | SEGMENT_EXECUTABLE)) {
        mapped_info_write->seg_type = CODE_SEGMENT;
        sprint("CODE_SEGMENT added at mapped info offset:%d\n", j);
      } else if (ph_addr->flags == (SEGMENT_READABLE | SEGMENT_WRITABLE)) {
        mapped_info_write->seg_type = DATA_SEGMENT;
        sprint("DATA_SEGMENT added at mapped info offset:%d\n", j);
      } else {
        panic("unknown program segment encountered, segment flag:%d.\n",
              ph_addr->flags);
      }
      target_ps->total_mapped_region++;
      break;
    }
  }
  return EL_OK;
}

//
// init elf_ctx, a data structure that loads the elf.
//
elf_status elf_init(elf_ctx *ctx, void *info) {
  ctx->info = info;

  // load the elf header
  if (elf_fpread(ctx, &ctx->ehdr, sizeof(ctx->ehdr), 0) != sizeof(ctx->ehdr))
    return EL_EIO;

  // check the signature (magic value) of the elf
  if (ctx->ehdr.magic != ELF_MAGIC)
    return EL_NOTELF;

  return EL_OK;
}

// leb128 (little-endian base 128) is a variable-length
// compression algoritm in DWARF
void read_uleb128(uint64 *out, char **off) {
  uint64 value = 0;
  int shift = 0;
  uint8 b;
  for (;;) {
    b = *(uint8 *)(*off);
    (*off)++;
    value |= ((uint64)b & 0x7F) << shift;
    shift += 7;
    if ((b & 0x80) == 0)
      break;
  }
  if (out)
    *out = value;
}
void read_sleb128(int64 *out, char **off) {
  int64 value = 0;
  int shift = 0;
  uint8 b;
  for (;;) {
    b = *(uint8 *)(*off);
    (*off)++;
    value |= ((uint64_t)b & 0x7F) << shift;
    shift += 7;
    if ((b & 0x80) == 0)
      break;
  }
  if (shift < 64 && (b & 0x40))
    value |= -(1 << shift);
  if (out)
    *out = value;
}
// Since reading below types through pointer cast requires aligned address,
// so we can only read them byte by byte
void read_uint64(uint64 *out, char **off) {
  *out = 0;
  for (int i = 0; i < 8; i++) {
    *out |= (uint64)(**off) << (i << 3);
    (*off)++;
  }
}
void read_uint32(uint32 *out, char **off) {
  *out = 0;
  for (int i = 0; i < 4; i++) {
    *out |= (uint32)(**off) << (i << 3);
    (*off)++;
  }
}
void read_uint16(uint16 *out, char **off) {
  *out = 0;
  for (int i = 0; i < 2; i++) {
    *out |= (uint16)(**off) << (i << 3);
    (*off)++;
  }
}

/*
 * analyzis the data in the debug_line section
 *
 * the function needs 3 parameters: elf context, data in the debug_line section
 * and length of debug_line section
 *
 * make 3 arrays:
 * "process->dir" stores all directory paths of code files
 * "process->file" stores all code file names of code files and their directory
 * path index of array "dir" "process->line" stores all relationships map
 * instruction addresses to code line numbers and their code file name index of
 * array "file"
 */
void make_addr_line(elf_ctx *ctx, char *debug_line, uint64 length) {
  // 在debug_line之后为pDir,、pFile、和pLine分配空间。
  // 当然这些空间也可以作为独立的结构，预先在内存中分配好
  // 这么写可读性比较差，篇幅关系懒得改了
  char **pDir = (char **)((((uint64)debug_line + length + 7) >> 3) << 3);
  code_file *pFile = (code_file *)(pDir + 64);
  addr_line *pLine = (addr_line *)(pFile + 64);

  int directory_count = 0;
  int file_count = 0;
  int line_count = 0;
  char *debugline_offset = debug_line;
  while (debugline_offset < debug_line + length) {
    // 在编译的过程中，每一个编译单元，都会在debugline中形成它自身的文件名-目录表。
    // 所以说，在我们保存的文件名总表和目录总表中，会有重复的文件名或目录。
    // 这里的dir_base就记录了当前编译单元的目录，在目录总表pDir下的开始位置。
    // file_base同理记录了当前编译单元的文件，在文件总表pFile下的开始位置。
    int dir_base = directory_count;
    int file_base = file_count;

    // 从debug_line中解析debug_header
    debug_header *dh = (debug_header *)debugline_offset;
    debugline_offset += sizeof(debug_header);

    // 从debugline中解析所有directory_name， 直到出现两个'\0'作为终止记号
    while (*debugline_offset != 0) {
      pDir[directory_count++] = debugline_offset;
      while (*debugline_offset != 0)
        debugline_offset++;
      debugline_offset++;
    }
    debugline_offset++;

    while (*debugline_offset != 0) {
      // 为每个文件确认与文件名的对应关系
      pFile[file_count].file = debugline_offset;
      while (*debugline_offset != 0)
        debugline_offset++;
      debugline_offset++;

      // 这里的dir记录的是文件在当前编译单元中，对应的目录序号(从1开始)。所以说在保存它与目录总表的对应关系时，需要做一个序号转换(从dir_base开始)
      uint64 dir;
      read_uleb128(&dir, &debugline_offset);
      // sprint("%s.dirvalue = %d;\n", pFile[file_count].file, dir);
      // sprint("%s.recdir = %d;\n", pFile[file_count].file, dir - 1 +
      // dir_base);
      pFile[file_count++].dir = dir - 1 + dir_base;
      read_uleb128(NULL, &debugline_offset);
      read_uleb128(NULL, &debugline_offset);
    }
    debugline_offset++;

    // 接下来解析的是指令与行的对应关系。
    // 由于时间精力有限，不做更多分析。
    addr_line regs;
    regs.addr = 0;
    regs.file = 1;
    regs.line = 1;
    // simulate the state machine op code
    for (;;) {
      uint8 op = *(debugline_offset++);
      switch (op) {
      case 0: // Extended Opcodes
        read_uleb128(NULL, &debugline_offset);
        op = *(debugline_offset++);
        switch (op) {
        case 1: // DW_LNE_end_sequence
          if (line_count > 0 && pLine[line_count - 1].addr == regs.addr)
            line_count--;
          pLine[line_count] = regs;
          pLine[line_count].file += file_base - 1;
          line_count++;
          goto endop;
        case 2: // DW_LNE_set_address
          read_uint64(&regs.addr, &debugline_offset);
          break;
        // ignore DW_LNE_define_file
        case 4: // DW_LNE_set_discriminator
          read_uleb128(NULL, &debugline_offset);
          break;
        }
        break;
      case 1: // DW_LNS_copy
        if (line_count > 0 && pLine[line_count - 1].addr == regs.addr)
          line_count--;
        pLine[line_count] = regs;
        pLine[line_count].file += file_base - 1;
        line_count++;
        break;
      case 2: { // DW_LNS_advance_pc
        uint64 delta;
        read_uleb128(&delta, &debugline_offset);
        regs.addr += delta * dh->min_instruction_length;
        break;
      }
      case 3: { // DW_LNS_advance_line
        int64 delta;
        read_sleb128(&delta, &debugline_offset);
        regs.line += delta;
        break;
      }
      case 4: // DW_LNS_set_file
        read_uleb128(&regs.file, &debugline_offset);
        break;
      case 5: // DW_LNS_set_column
        read_uleb128(NULL, &debugline_offset);
        break;
      case 6: // DW_LNS_negate_stmt
      case 7: // DW_LNS_set_basic_block
        break;
      case 8: { // DW_LNS_const_add_pc
        int adjust = 255 - dh->opcode_base;
        int delta = (adjust / dh->line_range) * dh->min_instruction_length;
        regs.addr += delta;
        break;
      }
      case 9: { // DW_LNS_fixed_advanced_pc
        uint16 delta;
        read_uint16(&delta, &debugline_offset);
        regs.addr += delta;
        break;
      }
        // ignore 10, 11 and 12
      default: { // Special Opcodes
        int adjust = op - dh->opcode_base;
        int addr_delta = (adjust / dh->line_range) * dh->min_instruction_length;
        int line_delta = dh->line_base + (adjust % dh->line_range);
        regs.addr += addr_delta;
        regs.line += line_delta;
        if (line_count > 0 && pLine[line_count - 1].addr == regs.addr)
          line_count--;
        pLine[line_count] = regs;
        pLine[line_count].file += file_base - 1;
        line_count++;
        break;
      }
      }
    }
  endop:;
  }
  // 在进程控制块中保存解析结果
  process *p = ((elf_info *)ctx->info)->p;
  p->debugline = debug_line;
  p->dir = pDir;
  p->file = pFile;
  p->line = pLine;
  p->line_count = line_count;

  // for (int i = 0; i < p->line_count; i++)
  // sprint("%p %d %d\n", p->line[i].addr, p->line[i].line, p->line[i].file);
}

//
// load the elf segments to memory regions.
//
elf_status elf_load(elf_ctx *ctx) {
  // traverse the elf program segment headers
  // sprint("%x\n", ctx->ehdr.phoff);
  for (int i = 0, offset = ctx->ehdr.phoff; i < ctx->ehdr.phnum;
       i++, offset += sizeof(elf_prog_header)) {
    // elf_prog_header structure is defined in kernel/elf.h
    elf_prog_header ph_addr;

    // read segment headers
    if (elf_fpread(ctx, (void *)&ph_addr, sizeof(ph_addr), offset) !=
        sizeof(ph_addr))
      return EL_EIO;

    if (ph_addr.type != ELF_PROG_LOAD)
      continue;
    if (ph_addr.memsz < ph_addr.filesz)
      return EL_ERR;
    if (ph_addr.vaddr + ph_addr.memsz < ph_addr.vaddr)
      return EL_ERR;

    elf_status ret;
    if ((ret = elf_load_segment(ctx, &ph_addr)) != EL_OK) {
      return ret;
    }
  }

  // elf_sect_header symbol_section_header;
  // elf_sect_header string_section_header;
  // elf_sect_header shstr_section_header;
  // uint64 shstr_offset;
  // shstr_offset = ctx->ehdr.shoff + ctx->ehdr.shstrndx * sizeof(elf_sect_header);
  // elf_fpread(ctx, (void *)&shstr_section_header, sizeof(shstr_section_header),
  //            shstr_offset);
  // char shstr_buffer[256 * 100];
	// shstr_buffer[256*100 -1 ] = '\0';
  // elf_fpread(ctx, &shstr_buffer, shstr_section_header.size,
  //            shstr_section_header.offset);
	// process *target_ps = ((elf_info *)ctx->info)->p;
  // for (int i = 0, offset = ctx->ehdr.shoff; i < ctx->ehdr.shnum;
  //      i++, offset += sizeof(elf_sect_header)) {
		
  //   elf_sect_header sh;
  //   if (elf_fpread(ctx, (void *)&sh, sizeof(sh), offset) != sizeof(sh))
  //     return EL_EIO;
  //   if (strcmp(shstr_buffer + sh.name, ".sdata") == 0) {
	// 		sprint("i=%d,ctx->ehdr.shnum=%d!\n",i,ctx->ehdr.shnum);
	// 		target_ps->trapframe->regs.gp = sh.addr + 0x800;
	// 		sprint("Found .sdata at: 0x%lx, setting gp to 0x%lx\n", sh.addr, target_ps->trapframe->regs.gp);
  //     //((elf_info *)ctx->info)->p->ktrapframe->regs.gp = sh.addr + 0x800;
  //     //sprint("Found .sdata at: 0x%lx, setting gp to 0x%lx\n", sh.addr,((elf_info *)ctx->info)->p->ktrapframe->regs.gp);
  //   }
  // }
  return EL_OK;
}

/* lab1_challenge1 & lab1_challenge2 */
// int ret;
// if ((ret = load_debug_infomation(ctx)) != EL_OK)
// return ret;

//
// load the elf of user application, by using the spike file interface.
//
void load_elf_from_file(process *p, char *filename) {
  int hartid = read_tp();

  // Sprint("Application: %s\n", arg_bug_msg.argv[hartid]);
  Sprint("Application: %s\n", filename);
  // elf loading. elf_ctx is defined in kernel/elf.h, used to track the loading
  // process.
  elf_ctx elfloader;
  // elf_info is defined above, used to tie the elf file and its corresponding
  // process.
  elf_info info;

  info.f = spike_file_open(filename, O_RDONLY, 0);
  info.p = p;

  // IS_ERR_VALUE is a macro defined in spike_interface/spike_htif.h
  if (IS_ERR_VALUE(info.f))
    panic("Fail on openning the input application program.\n");

  // init elfloader context. elf_init() is defined above.
  if (elf_init(&elfloader, &info) != EL_OK)
    panic("fail to init elfloader.\n");

  // load elf. elf_load() is defined above.
  if (elf_load(&elfloader) != EL_OK)
    panic("Fail on loading elf.\n");

  // entry (virtual, also physical in lab1_x) address
  p->trapframe->epc = elfloader.ehdr.entry;

  // close the host spike file
  spike_file_close(info.f);

  Sprint("Application program entry point (virtual address): 0x%lx\n",
         p->trapframe->epc);
}

// lab1_challenge1
elf_symbol function_symbols[SYMBOL_NUM];
char function_names[SYMBOL_NUM][SYMBOL_LENGTH];
int function_count;

// lab1_challenge2
elf_sect_header debugline_section_header;
char dbline_buf[8000];

elf_status load_debug_infomation(elf_ctx *ctx) {
  elf_sect_header symbol_section_header;
  elf_sect_header string_section_header;
  elf_sect_header shstr_section_header;
  uint64 shstr_offset;

  shstr_offset = ctx->ehdr.shoff + ctx->ehdr.shstrndx * sizeof(elf_sect_header);
  elf_fpread(ctx, (void *)&shstr_section_header, sizeof(shstr_section_header),
             shstr_offset);

  char shstr_buffer[256 * 100];
  elf_fpread(ctx, &shstr_buffer, shstr_section_header.size,
             shstr_section_header.offset);

  elf_sect_header temp_sh;
  for (int i = 0; i < ctx->ehdr.shnum; i++) {
    elf_fpread(ctx, (void *)&temp_sh, sizeof(temp_sh),
               ctx->ehdr.shoff + i * ctx->ehdr.shentsize);
    uint32 type = temp_sh.type;
    if (type == ELF_SHT_SYMTAB) {
      symbol_section_header = temp_sh;
    } else if (type == ELF_SHT_STRTAB &&
               strcmp(shstr_buffer + temp_sh.name, ".strtab") == 0) {
      string_section_header = temp_sh;
      // lab1_challenge2新增
    } else if (strcmp(shstr_buffer + temp_sh.name, ".debug_line") == 0) {
      debugline_section_header = temp_sh;
      elf_fpread(ctx, (void *)&dbline_buf, debugline_section_header.size,
                 debugline_section_header.offset);
      make_addr_line(ctx, dbline_buf, debugline_section_header.size);
    }
  }
  // print_elf_section_header(&debugline_section_header,shstr_buffer);
  // print_chars(dbline_buf,debugline_section_header.size);

  int count = 0;
  int symbol_num = symbol_section_header.size / sizeof(elf_symbol);
  for (int i = 0; i < symbol_num; i++) {
    elf_symbol symbol;
    elf_fpread(ctx, (void *)&symbol, sizeof(symbol),
               symbol_section_header.offset + i * sizeof(elf_symbol));
    if (symbol.name == 0)
      continue;
    if (symbol.info == 18) {
      char symbol_name[256];
      elf_fpread(ctx, (void *)symbol_name, sizeof(symbol_name),
                 string_section_header.offset + symbol.name);
      function_symbols[count] = symbol;
      strcpy(function_names[count++], symbol_name);
      // print_elf_symbol(&symbol,count - 1);
    }
  }
  function_count = count;

  return EL_OK;
}

char *locate_function_name(uint64 epc) {
  int find_index = 0;
  uint64 closest_entry = 0x0;
  for (int i = 0; i < function_count; i++) {
    uint64 function_entry = function_symbols[i].value;
    if (function_entry < epc && function_entry > closest_entry) {
      closest_entry = function_entry;
      find_index = i;
    }
  }
  return function_names[find_index];
}

// debug函数，查看elf符号信息
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

// debug函数，查看elf节头信息
//  打印节类型字符串
const char *get_section_type_string(uint32 type) {
  switch (type) {
  case ELF_SHT_NULL:
    return "NULL";
  case ELF_SHT_PROGBITS:
    return "PROGBITS";
  case ELF_SHT_SYMTAB:
    return "SYMTAB";
  case ELF_SHT_STRTAB:
    return "STRTAB";
  case ELF_SHT_RELA:
    return "RELA";
  case ELF_SHT_HASH:
    return "HASH";
  case ELF_SHT_DYNAMIC:
    return "DYNAMIC";
  case ELF_SHT_NOTE:
    return "NOTE";
  case ELF_SHT_NOBITS:
    return "NOBITS";
  case ELF_SHT_REL:
    return "REL";
  case ELF_SHT_SHLIB:
    return "SHLIB";
  case ELF_SHT_DYNSYM:
    return "DYNSYM";
  default:
    return "UNKNOWN";
  }
}

// 打印节标志位
const char *get_section_flags_string(uint64 flags) {

  if (flags & ELF_SHF_WRITE)
    return "WRITE ";
  if (flags & ELF_SHF_ALLOC)
    return "ALLOC ";
  if (flags & ELF_SHF_EXECINSTR)
    return "EXECINSTR ";

  return "NONE/OTHERS";
}

// 打印 ELF 节头信息
void print_elf_section_header(elf_sect_header *section_header, char *shstr) {
  sprint("%-20s %-10s %-10s %-10s %-10s %-10s %-10s %-10s %-10s\n",
         "Section Name", "Type", "Flags", "Addr", "Offset", "Size", "Link",
         "Info", "Align");

  // 获取节名称，假设节名称表在字符串表中
  const char *section_name =
      (section_header->name != 0) ? (shstr + section_header->name) : "None";

  // 打印节头信息
  sprint("%-20s %-10s %-10s 0x%lx 0x%lx 0x%lx %-10u %-10u %-10lu\n",
         section_name, get_section_type_string(section_header->type),
         get_section_flags_string(section_header->flags), section_header->addr,
         section_header->offset, section_header->size, section_header->link,
         section_header->info, section_header->addralign);
}

void print_chars(const char *start, int n) {
  for (int i = 0; i < n; i++) {
    char tmp = *(start + i);
    if (tmp) {
      sprint("%c", tmp);
    } else {
      sprint("\0");
    }
  }
}