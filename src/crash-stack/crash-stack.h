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

typedef struct Mapping
{
  uintptr_t m_start;
  uintptr_t m_end;
  uintptr_t m_offset;
  const char *m_name;
  int m_fd;
  Elf *m_elf;
} Mapping;

#define MAX_MAPPINGS_NUM 1000

typedef struct Mappings
{
  Mapping tab[MAX_MAPPINGS_NUM];
  size_t elems;
} Mappings;

void *get_place_for_register_value (const char *regname, int regnum);
void create_crash_stack (Dwfl *dwfl, Elf *core, pid_t pid, Mappings *mappings, Callstack *callstack);

Dwarf_Addr crash_stack_libelf_get_prologue_pc (Dwfl *dwfl, Dwarf_Addr current_pc, Mappings *mappings);
bool crash_stack_libelf_read_value (Dwfl *dwfl, Elf *core, pid_t pid,
                                    Dwarf_Addr a, void *v, size_t size,
                                    Mappings *mappings);

void *crash_stack_get_memory_for_ptrace_registers (size_t *size);
void crash_stack_set_ptrace_registers (void *regbuf);

#endif /* CRASH_STACK_H */
