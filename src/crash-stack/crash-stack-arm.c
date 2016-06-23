#include "crash-stack.h"
#include "wind/unwarm.h"

#include <string.h>
#include <sys/ptrace.h>
#include <sys/user.h>

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

static struct user_regs g_ptrace_registers;

void *crash_stack_get_memory_for_ptrace_registers (size_t *size)
{
  if (NULL != size)
    *size = sizeof (g_ptrace_registers);
  return &g_ptrace_registers;
}

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

void crash_stack_set_ptrace_registers (void *regbuf)
{
  struct user_regs *registers = regbuf;
  int i;
  for (i = 0; i < sizeof(registers->uregs)/sizeof(registers->uregs[0]); i++)
  {
    void *regmem = get_place_for_register_value ("", i);
    if (NULL != regmem)
      memcpy(regmem, &registers->uregs[i], sizeof(registers->uregs[i]));
  }
}

static Boolean report (void *data, Int32 address)
{
  Callstack *callstack = (Callstack *)(data);
  callstack->tab[callstack->elems++] = address;

  return callstack->elems < MAX_CALLSTACK_LEN ? TRUE : FALSE;
}

Boolean readT (Int32 a, void *v, size_t size)
{
  return crash_stack_libelf_read_value (g_dwfl, g_core, g_pid, a, v, size, g_mappings);
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
    return crash_stack_libelf_get_prologue_pc (g_dwfl, current_pc, g_mappings);
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

