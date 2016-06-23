#include "crash-stack.h"
#include <elfutils/libdwfl.h>
#include <elfutils/version.h>
#include <string.h>

typedef union
{
  uint16_t reg16;
  uint32_t reg32;
  uint64_t reg64;
} Register;

static Register g_pc;
static Register g_sp;

#if _ELFUTILS_PREREQ(0,158)
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
#endif

static const char *pc_names[] =
{
  "pc", "rip", "eip", "ip"
};

static const char *sp_names[] =
{
  "sp", "rsp", "esp"
};

static bool is_in (const char *name, const char **names, int elems)
{
  int nit;
  for (nit = 0; nit < elems; ++nit)
  {
    if (strcmp (name, names[nit]) == 0)
      return true;
  }
  return false;
}

#define IS_IN(name,names) is_in((name), (names), sizeof(names)/sizeof(names[0]))

void *get_place_for_register_value (const char *regname, int regnum)
{
  if (IS_IN(regname, pc_names)) return &g_pc;
  else if (IS_IN(regname, sp_names)) return &g_sp;

  return 0;
}

void create_crash_stack (Dwfl *dwfl, Elf *core, pid_t pid, Mappings *mappings, Callstack *callstack)
{
  callstack->elems = 0;
#if _ELFUTILS_PREREQ(0,158)
  dwfl_getthreads (dwfl, thread_callback, callstack);
#endif
}

