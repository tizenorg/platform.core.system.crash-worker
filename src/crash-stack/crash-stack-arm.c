#include "crash-stack.h"
#include "wind/unwarm.h"

#include <string.h>
#include <sys/ptrace.h>

static Elf *g_core = NULL;
static Dwfl *g_dwfl = NULL;
static Mappings *g_mappings = NULL;
static pid_t g_pid = 0;

struct Regs
{
  Dwarf_Addr regs[REGS_REGULAR_NUM];
  Dwarf_Addr sp;
  Dwarf_Addr lr;
  Dwarf_Addr pc;
  Dwarf_Addr spsr;
};

typedef struct Regs Regs;
static Regs g_regs;

void *get_place_for_register_value (const char *regname, int regnum)
{
  if (strcmp (regname, "pc") == 0 || REG_PC == regnum)
  {
    return &g_regs.pc;
  }
  else if (strcmp (regname, "sp") == 0 || REG_SP == regnum)
  {
    return &g_regs.sp;
  }
  else if (strcmp (regname, "lr") == 0 || REG_LR == regnum)
  {
    return &g_regs.lr;
  }
  else if (strcmp (regname, "spsr") == 0 || REG_SPSR == regnum)
  {
    return &g_regs.spsr;
  }
  else if (regnum < REGS_REGULAR_NUM)
  {
    return &g_regs.regs[regnum];
  }
  return NULL;
}

static Boolean report (void *data, Int32 address)
{
  Callstack *callstack = (Callstack *)(data);
  callstack->tab[callstack->elems++] = address;

  return callstack->elems < MAX_CALLSTACK_LEN ? TRUE : FALSE;
}

Boolean readT (Int32 a, void *v, size_t size)
{
  Dwfl_Module *module = 0;
  Elf_Data *data = NULL;

  int segment = dwfl_addrsegment (g_dwfl, a, &module);

  if (module != NULL)
  {
    Dwarf_Addr start;
    dwfl_module_info (module, NULL, &start, NULL, NULL, NULL, NULL, NULL);

    GElf_Addr bias;
    Elf *elf = dwfl_module_getelf (module, &bias);

    data = elf_getdata_rawchunk (elf, a-start, size, ELF_T_BYTE);
  }
  if (NULL == data && segment != -1)
  {
    // get data from segment
    GElf_Phdr mem;
    GElf_Phdr *phdr = gelf_getphdr (g_core, segment, &mem);
    Dwarf_Addr offset_in_segment = a - phdr->p_vaddr;
    if (offset_in_segment < phdr->p_filesz)
    {
      Dwarf_Addr offset_in_file = phdr->p_offset + offset_in_segment;

      data = elf_getdata_rawchunk (g_core, offset_in_file, size, ELF_T_BYTE);
    }
  }

  if (NULL == data && module != NULL)
  {
    const char *name = dwfl_module_info (module, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    if (name != NULL && name[0] == '[')
    {
      int i;
      // get module from mappings
      for (i = 0; i < g_mappings->elems; i++)
      {
        if (g_mappings->tab[i].m_start <= a && a < g_mappings->tab[i].m_end)
        {
          // compute offset relative to the start of the mapping
          Int32 offset = a - g_mappings->tab[i].m_start;
          // read from the file, but also account file offset
          data = elf_getdata_rawchunk (g_mappings->tab[i].m_elf,
              offset + g_mappings->tab[i].m_offset, size, ELF_T_BYTE);
          break;
        }
      }
    }
  }

  if (data != NULL)
  {
    memcpy (v, data->d_buf, size);
    return TRUE;
  }

  /* Still no data, but we have a process - read memory with ptrace */
  if (NULL == data && g_pid > 1)
  {
     long val = ptrace (PTRACE_PEEKDATA, g_pid, a, NULL);
     memcpy (v, &val, size);
     return TRUE;
  }

  return FALSE;
}

static Boolean readW (Int32 a, Int32 *v)
{
  return readT(a,v,sizeof(*v));
}

static Boolean readH (Int32 a, Int16 *v)
{
  return readT(a,v,sizeof(*v));
}

static Boolean readB (Int32 a, Int8 *v)
{
  return readT(a,v,sizeof(*v));
}

static Int32 getProloguePC (Int32 current_pc)
{
    Int32 result = 0;
    Dwfl_Module *module = dwfl_addrmodule (g_dwfl, current_pc);
    if (module)
    {
//        GElf_Off offset;
        GElf_Sym sym;
//        dwfl_module_addrinfo (module, current_pc, &offset, &sym, NULL, NULL, NULL);
        dwfl_module_addrsym (module, current_pc, &sym, NULL);
//        result = current_pc - offset;
        result = sym.st_value;
    }
    if (0 == result)
    {
      int i;
      for (i=0; i < g_mappings->elems; i++)
      {
        if (g_mappings->tab[i].m_start <= current_pc && current_pc < g_mappings->tab[i].m_end)
        {
          /* go through symbols to find the nearest */
          Elf_Scn *scn = NULL;
          Elf *elf = g_mappings->tab[i].m_elf;
          while ((scn = elf_nextscn (elf, scn)) != NULL)
          {
            GElf_Shdr shdr_mem;
            GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
            if (shdr != NULL && (shdr->sh_type == SHT_SYMTAB || shdr->sh_type == SHT_DYNSYM))
            {
              Elf_Data *sdata = elf_getdata (scn, NULL);
              unsigned int nsyms = sdata->d_size / (gelf_getclass(elf) == ELFCLASS32 ?
                  sizeof (Elf32_Sym) :
                  sizeof (Elf64_Sym));
              unsigned int cnt;
              uintptr_t address_offset = current_pc;
              if (shdr->sh_type == SHT_DYNSYM)
                address_offset -= g_mappings->tab[i].m_start;
              for (cnt = 0; cnt < nsyms; ++cnt)
              {
                GElf_Sym sym_mem;
                Elf32_Word xndx;
                GElf_Sym *sym = gelf_getsymshndx(sdata, NULL, cnt, &sym_mem, &xndx);
                if (sym != NULL && sym->st_shndx != SHN_UNDEF)
                {
                  if (sym->st_value <= address_offset && address_offset < sym->st_value + sym->st_size)
                  {
                    return sym->st_value;
                  }
                }
              }
            }
          }
        }
      }
    }
    return result;
}

void create_crash_stack (Dwfl *dwfl, Elf *core, pid_t pid, Mappings *mappings, Callstack *callstack)
{
  UnwindCallbacks callbacks =
  {
    report,
    readW,
    readH,
    readB,
    getProloguePC
#ifdef UNW_DEBUG
    ,
    printf
#endif
  };
  UnwState state;

  g_dwfl = dwfl;
  g_core = core;
  g_mappings = mappings;
  g_pid = pid;

  callstack->tab[0] = g_regs.pc;
  callstack->elems = 1;

  UnwInitState (&state, &callbacks, callstack, g_regs.pc, g_regs.sp);
  int i;
  for (i = 0; i < REGS_REGULAR_NUM; i++)
  {
    state.regData[i].v = g_regs.regs[i];
    state.regData[i].o = REG_VAL_FROM_CONST;
  }
  state.regData[REG_LR].v = g_regs.lr;
  state.regData[REG_LR].o = REG_VAL_FROM_STACK;
  state.regData[REG_SPSR].v = g_regs.spsr;
  state.regData[REG_SPSR].o = REG_VAL_FROM_CONST;

  if (UnwIsAddrThumb (g_regs.pc, g_regs.spsr))
    UnwStartThumb (&state);
  else
    UnwStartArm (&state);
}

