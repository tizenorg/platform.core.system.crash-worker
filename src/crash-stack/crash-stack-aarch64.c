#include "crash-stack.h"
#include <sys/user.h>
#include <string.h>

static struct user_regs_struct g_registers;

struct Regs
{
	Dwarf_Addr x29;
	Dwarf_Addr x30;
	Dwarf_Addr pc;
	Dwarf_Addr sp;
};

#define REG_SP 32
#define REG_PC 33
#define REG_X29 29
#define REG_X30 30

typedef struct Regs Regs;
static Regs g_regs;

void *crash_stack_get_memory_for_ptrace_registers (size_t *size)
{
  if (NULL != size)
    *size = sizeof (g_registers);
  return &g_registers;
}

void crash_stack_set_ptrace_registers (void *regbuf)
{
  struct user_regs_struct *regs = regbuf;

  memcpy (get_place_for_register_value ("sp", 0), &regs->sp, sizeof (regs->sp));
  memcpy (get_place_for_register_value ("pc", 0), &regs->pc, sizeof (regs->pc));
  memcpy (get_place_for_register_value ("x29", 0), &regs->regs[29], sizeof (regs->regs[29]));
  memcpy (get_place_for_register_value ("x30", 0), &regs->regs[30], sizeof (regs->regs[30]));
}

void create_crash_stack (Dwfl *dwfl, Elf *core, pid_t pid, Mappings *mappings, Callstack *callstack)
{
	callstack->elems = 0;
	callstack->tab[callstack->elems++] = g_regs.pc;
	callstack->tab[callstack->elems++] = g_regs.x30;

	bool end = false;

	do
	{
		uint64_t newx29, newx30;
		bool read29 = crash_stack_libelf_read_value (dwfl, core, pid,
				g_regs.x29,
				&newx29, sizeof (newx29), mappings);
		bool read30 = crash_stack_libelf_read_value (dwfl, core, pid,
				g_regs.x29 + sizeof(newx29),
				&newx30, sizeof (newx30), mappings);
		if (read29 && read30)
		{
			callstack->tab[callstack->elems++] = newx30;
			g_regs.x29 = newx29;
		}
		else end = true;
	} while (!end);
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
	else if (strcmp (regname, "x29") == 0 || REG_X29 == regnum)
	{
		return &g_regs.x29;
	}
	else if (strcmp (regname, "x30") == 0 || REG_X30 == regnum)
	{
		return &g_regs.x30;
	}
	return NULL;
}
