#include "crash-stack.h"
#include "wind/unwarm.h"

#include <string.h>

static Elf *g_core = NULL;
static Dwfl *g_dwfl = NULL;

struct Regs
{
  Dwarf_Addr regs[REGS_REGULAR_NUM];
  Dwarf_Addr sp;
  Dwarf_Addr lr;
  Dwarf_Addr pc;
  Dwarf_Addr spsr;
};

static Regs g_regs;

Regs *get_regs_struct (void)
{
  return &g_regs;
}

void *get_place_for_register_value (const char *regname, int regnum)
{
  if (strcmp (regname, "pc") == 0)
  {
    return &g_regs.pc;
  }
  else if (strcmp (regname, "sp") == 0)
  {
    return &g_regs.sp;
  }
  else if (strcmp (regname, "lr") == 0)
  {
    return &g_regs.lr;
  }
  else if (strcmp (regname, "spsr") == 0)
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
  Boolean result = FALSE;

  int segment = dwfl_addrsegment (g_dwfl, a, &module);

  if (module != NULL)
  {
    Dwarf_Addr start;
    dwfl_module_info (module, NULL, &start, NULL, NULL, NULL, NULL, NULL);

    GElf_Addr bias;
    Elf *elf = dwfl_module_getelf (module, &bias);

    Elf_Data *data = elf_getdata_rawchunk (elf, a-start, size, ELF_T_BYTE);
    if (data != NULL)
    {
      memcpy (v, data->d_buf, size);
      result = TRUE;
    }
  }
  if (!result && segment != -1)
  {
    // get data from segment
    GElf_Phdr mem;
    GElf_Phdr *phdr = gelf_getphdr (g_core, segment, &mem);
    Dwarf_Addr offset_in_segment = a - phdr->p_vaddr;
    Dwarf_Addr offset_in_file = phdr->p_offset + offset_in_segment;

    Elf_Data *data = elf_getdata_rawchunk (g_core, offset_in_file, size, ELF_T_BYTE);
    if (data != NULL)
    {
      memcpy (v, data->d_buf, size);
      result = TRUE;
    }
  }

  return result;
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
        GElf_Off offset;
        GElf_Sym sym;
        dwfl_module_addrinfo (module, current_pc, &offset, &sym, NULL, NULL, NULL);
        result = current_pc - offset;
    }
    return result;
}

void create_crash_stack (Regs *regs, Dwfl *dwfl, Elf *core, Callstack *callstack)
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

  callstack->tab[0] = regs->pc;
  callstack->elems = 1;

  UnwInitState (&state, &callbacks, callstack, regs->pc, regs->sp);
  int i;
  for (i = 0; i < REGS_REGULAR_NUM; i++)
  {
    state.regData[i].v = regs->regs[i];
    state.regData[i].o = REG_VAL_FROM_CONST;
  }
  state.regData[REG_LR].v = regs->lr;
  state.regData[REG_LR].o = REG_VAL_FROM_STACK;
  state.regData[REG_SPSR].v = regs->spsr;
  state.regData[REG_SPSR].o = REG_VAL_FROM_CONST;

  if (UnwIsAddrThumb (regs->pc, regs->spsr))
    UnwStartThumb (&state);
  else
    UnwStartArm (&state);
}

