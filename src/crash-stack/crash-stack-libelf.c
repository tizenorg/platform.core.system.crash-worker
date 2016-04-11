#include "crash-stack.h"
#include <elfutils/libdwfl.h>

static int frame_callback (Dwfl_Frame *state, void *arg)
{
  Callstack *callstack = (Callstack*)arg;
  Dwarf_Addr address;
  dwfl_frame_pc (state, &address, NULL);
  callstack->tab[callstack->elems++] = address;
  return callstack->elems < MAX_CALLSTACK_LEN ? DWARF_CB_OK : DWARF_CB_ABORT;
}

static int thread_callback (Dwfl_Thread *thread, void *thread_arg)
{
  dwfl_thread_getframes (thread, frame_callback, thread_arg);
  return DWARF_CB_ABORT;
}

Regs *get_regs_struct (void)
{
  return 0;
}

void *get_place_for_register_value (const char *regname, int regnum)
{
  return 0;
}

void create_crash_stack (Regs *regs, Dwfl *dwfl, Elf *core, Mappings *mappings, Callstack *callstack)
{
  callstack->elems = 0;
  dwfl_getthreads (dwfl, thread_callback, callstack);
}

