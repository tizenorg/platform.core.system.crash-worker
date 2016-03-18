#include "crash-stack.h"

Regs *get_regs_struct (void)
{
  return 0;
}

void *get_place_for_register_value (const char *regname, int regnum)
{
  return 0;
}

void create_crash_stack (Regs *regs, Dwfl *dwfl, Elf *core, Callstack *callstack)
{
}

