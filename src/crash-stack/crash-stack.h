#ifndef CRASH_STACK_H
#define CRASH_STACK_H

#include <stdint.h>
#include <elfutils/libdwfl.h>
#include <elfutils/libebl.h>

#define MAX_CALLSTACK_LEN 1000

typedef struct Callstack {
	uintptr_t tab[MAX_CALLSTACK_LEN];
	size_t elems;
} Callstack;

typedef struct Mapping {
	uintptr_t m_start;
	uintptr_t m_end;
	uintptr_t m_offset;
	const char *m_name;
	int m_fd;
	Elf *m_elf;
} Mapping;

#define MAX_MAPPINGS_NUM 1000

typedef struct Mappings {
	Mapping tab[MAX_MAPPINGS_NUM];
	size_t elems;
} Mappings;

void *get_place_for_register_value(const char *regname, int regnum);
void create_crash_stack(Dwfl *dwfl, Elf *core, pid_t pid, Mappings *mappings, Callstack *callstack);

#endif /* CRASH_STACK_H */
