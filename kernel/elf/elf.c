/*
 * routines that scan and load a (host) Executable and Linkable Format (ELF)
 * file into the (emulated) memory.
 */

#include <kernel/elf.h>
#include <kernel/pmm.h>
#include <kernel/riscv.h>
#include "spike_interface/spike_utils.h"
#include <util/string.h>
#include <kernel/vmm.h>
#include <kernel/user_mem.h>

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

elf_status elf_load_segment(elf_ctx *ctx, elf_prog_header *ph_addr) {
  process *target_ps = ((elf_info *)ctx->info)->p;
  size_t num_bytes = ph_addr->memsz;
  uint64 num_pages = (ph_addr->memsz + PGSIZE - 1) / PGSIZE; // 向上取整
  void *first_pa = NULL;
  for (uint64 i = 0; i < num_pages; i++) {
    void *pa = user_alloc_page(((elf_info *)ctx->info)->p,ph_addr->vaddr + i*PGSIZE, PROT_WRITE | PROT_READ | PROT_EXEC);
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

  // sprint("Application: %s\n", arg_bug_msg.argv[hartid]);
  sprint("Application: %s\n", filename);
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

  sprint("Application program entry point (virtual address): 0x%lx\n",
         p->trapframe->epc);
}

