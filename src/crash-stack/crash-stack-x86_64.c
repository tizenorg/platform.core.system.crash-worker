#include "crash-stack.h"
#include <sys/user.h>
#include <string.h>

struct user_regs_struct g_registers;

void *crash_stack_get_memory_for_ptrace_registers (size_t *size)
{
  if (NULL != size)
    *size = sizeof(g_registers);
  return &g_registers;
}

void crash_stack_set_ptrace_registers (void *regbuf)
{
  void *rsp = get_place_for_register_value ("rsp", 0);
  void *rip = get_place_for_register_value ("rip", 0);

  struct user_regs_struct *regs = regbuf;

  memcpy (rsp, &regs->rsp, sizeof (regs->rsp));
  memcpy (rip, &regs->rip, sizeof (regs->rip));
}
