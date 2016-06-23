#include "crash-stack.h"
#include <string.h>
#include <sys/ptrace.h>
#include <errno.h>

bool crash_stack_libelf_read_value (Dwfl *dwfl, Elf *core, pid_t pid,
                                    Dwarf_Addr a, void *v, size_t size,
                                    Mappings *mappings)
{
    Dwfl_Module *module = 0;
    Elf_Data *data = NULL;

    int segment = dwfl_addrsegment (dwfl, a, &module);

    if (module != NULL)
    {
      Dwarf_Addr start;
      dwfl_module_info (module, NULL, &start, NULL, NULL, NULL, NULL, NULL);

      GElf_Addr bias;
      Elf *elf = dwfl_module_getelf (module, &bias);

      data = elf_getdata_rawchunk (elf, a-start, size, ELF_T_BYTE);
    }
    if (NULL == data && segment != -1)
    {
      // get data from segment
      GElf_Phdr mem;
      GElf_Phdr *phdr = gelf_getphdr (core, segment, &mem);
      Dwarf_Addr offset_in_segment = a - phdr->p_vaddr;
      if (offset_in_segment < phdr->p_filesz)
      {
        Dwarf_Addr offset_in_file = phdr->p_offset + offset_in_segment;

        data = elf_getdata_rawchunk (core, offset_in_file, size, ELF_T_BYTE);
      }
    }

    if (NULL == data && module != NULL)
    {
      const char *name = dwfl_module_info (module, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
      if (name != NULL && name[0] == '[')
      {
        int i;
        // get module from mappings
        for (i = 0; i < mappings->elems; i++)
        {
          if (mappings->tab[i].m_start <= a && a < mappings->tab[i].m_end)
          {
            // compute offset relative to the start of the mapping
            long offset = a - mappings->tab[i].m_start;
            // read from the file, but also account file offset
            data = elf_getdata_rawchunk (mappings->tab[i].m_elf,
                offset + mappings->tab[i].m_offset, size, ELF_T_BYTE);
            break;
          }
        }
      }
    }

    if (data != NULL)
    {
      memcpy (v, data->d_buf, size);
      return true;
    }

    /* Still no data, but we have a process - read memory with ptrace */
    /* FIXME need to know if we are still in the mapped area */
    /* Bigger issue is that dwfl does not have modules */
    if (pid > 1)
    {
       long val = ptrace (PTRACE_PEEKDATA, pid, a, NULL);
       if (-1 == val && errno)
         return false;
       memcpy (v, &val, size);
       return true;
    }

    return false;
}

Dwarf_Addr crash_stack_libelf_get_prologue_pc (Dwfl *dwfl, Dwarf_Addr current_pc, Mappings *mappings)
{
    Dwarf_Addr result = 0;
    Dwfl_Module *module = dwfl_addrmodule (dwfl, current_pc);
    if (module)
    {
        GElf_Sym sym;
        dwfl_module_addrsym (module, current_pc, &sym, NULL);
        result = sym.st_value;
    }
    if (0 == result)
    {
      int i;
      for (i=0; i < mappings->elems; i++)
      {
        if (mappings->tab[i].m_start <= current_pc && current_pc < mappings->tab[i].m_end)
        {
          /* go through symbols to find the nearest */
          Elf_Scn *scn = NULL;
          Elf *elf = mappings->tab[i].m_elf;
          while ((scn = elf_nextscn (elf, scn)) != NULL)
          {
            GElf_Shdr shdr_mem;
            GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
            if (shdr != NULL && (shdr->sh_type == SHT_SYMTAB || shdr->sh_type == SHT_DYNSYM))
            {
              Elf_Data *sdata = elf_getdata (scn, NULL);
              unsigned int nsyms = sdata->d_size / (gelf_getclass(elf) == ELFCLASS32 ?
                  sizeof (Elf32_Sym) :
                  sizeof (Elf64_Sym));
              unsigned int cnt;
              uintptr_t address_offset = current_pc;
              if (shdr->sh_type == SHT_DYNSYM)
                address_offset -= mappings->tab[i].m_start;
              for (cnt = 0; cnt < nsyms; ++cnt)
              {
                GElf_Sym sym_mem;
                Elf32_Word xndx;
                GElf_Sym *sym = gelf_getsymshndx(sdata, NULL, cnt, &sym_mem, &xndx);
                if (sym != NULL && sym->st_shndx != SHN_UNDEF)
                {
                  if (sym->st_value <= address_offset && address_offset < sym->st_value + sym->st_size)
                  {
                    return sym->st_value;
                  }
                }
              }
            }
          }
        }
      }
    }
    return result;
}
