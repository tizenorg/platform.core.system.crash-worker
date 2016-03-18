#ifndef CRASH_STACK_H
#define CRASH_STACK_H

#include <stdint.h>
#include <elfutils/libdwfl.h>
#include <elfutils/libebl.h>

#define MAX_CALLSTACK_LEN 1000

typedef struct Callstack
{
  uintptr_t tab[MAX_CALLSTACK_LEN];
  size_t elems;
} Callstack;

struct Regs;
typedef struct Regs Regs;

Regs *get_regs_struct (void);
void *get_place_for_register_value (const char *regname, int regnum);
void create_crash_stack (Regs *regs, Dwfl *dwfl, Elf *core, Callstack *callstack);

#endif /* CRASH_STACK_H */
