#include <stdlib.h>
#include <stdio.h>
#include <libelf.h>
#include <elfutils/libdwfl.h>
#include <elfutils/libebl.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <linux/prctl.h>
#include "crash-stack.h"
#include <string.h>
#include <elfutils/version.h>

extern char *__cxa_demangle (const char *mangled_name, char *output_buffer,
			     size_t *length, int *status);

static int module_callback (Dwfl_Module *module, void **userdata,
						   const char *name, Dwarf_Addr address,
						   void *arg)
{
  if (name != NULL && name[0] == '[')
  {
    /* libdwfl couldn't get the module file - we will get it later from notes */
    Mappings *mappings = arg;
    if (mappings->elems < MAX_MAPPINGS_NUM)
    {
      size_t elems = mappings->elems;
      mappings->tab[elems].m_start = address;
      mappings->tab[elems].m_end = 0;
      mappings->tab[elems].m_offset = 0;
      mappings->tab[elems].m_name = NULL;
      mappings->tab[elems].m_fd = -1;
      mappings->tab[elems].m_elf = 0;
      mappings->elems++;
    }
  }
  return DWARF_CB_OK;
}

static void getvalue (Elf *core, const void *from, size_t size, void *to)
{
  Elf_Data out =
  {
    .d_buf = to,
    .d_type = size == 32 ? ELF_T_WORD : ELF_T_XWORD,
    .d_version = EV_CURRENT,
    .d_size = size/8,
    .d_off = 0,
    .d_align = 0
  };
  Elf_Data in =
  {
    .d_buf = (void*)(from),
    .d_type = out.d_type,
    .d_version = out.d_version,
    .d_size = out.d_size,
    .d_off = 0,
    .d_align = 0
  };
  Elf_Data *data;
  if (gelf_getclass (core) == ELFCLASS32)
    data = elf32_xlatetom (&out, &in, elf_getident (core, NULL)[EI_DATA]);
  else
    data = elf64_xlatetom (&out, &in, elf_getident (core, NULL)[EI_DATA]);
  if (data == NULL)
    fprintf (stderr, "failed to get value from core file\n");
}

static void updateMapping (Mappings *mappings, uint64_t mapping_start, uint64_t mapping_end,
    uint64_t offset, const char *name)
{
  int i;
  for (i = 0; i < mappings->elems; i++)
  {
    if (mappings->tab[i].m_start == mapping_start)
    {
      mappings->tab[i].m_end = mapping_end;
      mappings->tab[i].m_name = name;
      mappings->tab[i].m_offset = offset;
      mappings->tab[i].m_fd = open(name, O_RDONLY);
      mappings->tab[i].m_elf = elf_begin(mappings->tab[i].m_fd, ELF_C_READ_MMAP, NULL);
      return;
    }
  }
}

static void parse_note_file (Elf *elf, const char *desc, uint64_t *values_cnt, uint64_t *page_size,
    size_t *addr_size, const char **values, const char **filenames)
{
  *addr_size = gelf_fsize (elf, ELF_T_ADDR, 1, EV_CURRENT);
  getvalue(elf, desc, *addr_size*8, values_cnt);
  getvalue(elf, desc + *addr_size, *addr_size*8, page_size);
  /* First: triplets of <mapping-start> <mapping-end> <offset-in-pages>
   *     count = values_cnt
   * Then the names of files.
   */
  *values = desc + 2 * *addr_size;
  *filenames = *values + 3 * *addr_size * *values_cnt;
}

static void get_mapping_item(Elf *elf, size_t addr_size, const void *item,
    uint64_t *mapping_start, uint64_t *mapping_end, uint64_t *offset_in_pages)
{
  getvalue(elf, item, addr_size*8, mapping_start);
  getvalue(elf, item + addr_size, addr_size*8, mapping_end);
  getvalue(elf, item + 2 * addr_size, addr_size*8, offset_in_pages);
}

static char *try_symbol_from_elfs (Elf *core, Elf_Data *notes, uintptr_t address,
    const char **module_name)
{
  GElf_Nhdr nhdr;
  char *symbol = NULL;
  size_t pos = 0;
  size_t new_pos = 0;
  size_t name_pos;
  size_t desc_pos;

  while ((new_pos = gelf_getnote (notes, pos, &nhdr, &name_pos, &desc_pos)) > 0)
  {
    if (nhdr.n_type == NT_FILE)
    {
      uint64_t values_cnt, page_size;
      const char *values;
      const char *filenames;
      size_t addr_size;

      parse_note_file(core, notes->d_buf + desc_pos, &values_cnt, &page_size, &addr_size, &values, &filenames);

      int ii;
      for (ii = 0; ii < values_cnt; ii++)
      {
          uint64_t mapping_start, mapping_end, offset_in_pages;
          const char *item = values + 3 * addr_size * ii;

          get_mapping_item (core, addr_size, item, &mapping_start, &mapping_end, &offset_in_pages);

          if (mapping_start <= address && address < mapping_end)
          {
            Elf *elf;
            int fd;
            fd = open(filenames, O_RDONLY);
            if (-1 == fd)
              return NULL;

            elf = elf_begin (fd, ELF_C_READ_MMAP, NULL);

            if (NULL == elf)
              return NULL;

            Elf_Scn *scn = NULL;
            *module_name = filenames;

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
                uintptr_t address_offset = address;
                if (shdr->sh_type == SHT_DYNSYM)
                  address_offset -= mapping_start;
                for (cnt = 0; cnt < nsyms; ++cnt)
                {
                  GElf_Sym sym_mem;
                  Elf32_Word xndx;
                  GElf_Sym *sym = gelf_getsymshndx(sdata, NULL, cnt, &sym_mem, &xndx);
                  if (sym != NULL && sym->st_shndx != SHN_UNDEF)
                  {
                    if (sym->st_value <= address_offset && address_offset < sym->st_value + sym->st_size)
                    {
                      symbol = strdup(elf_strptr (elf, shdr->sh_link, sym->st_name));
                      break;
                    }
                  }
                }
              }
            }

            elf_end(elf);
            close(fd);
            return symbol;
          }

          filenames += strlen(filenames)+1;
      }
    }
    pos = new_pos;
  }

  return NULL;
}

int main(int argc, char **argv)
{
  prctl (PR_SET_DUMPABLE, 0);

  if (argc != 2)
  {
    fprintf (stderr, "Usage: %s <core-file>\n", argv[0]);
    return 1;
  }

  int core_fd = open (argv[1], O_RDONLY);
  if (core_fd < 0)
  {
    perror (argv[1]);
    return 2;
  }

  elf_version (EV_CURRENT);

  Elf *core = elf_begin (core_fd, ELF_C_READ_MMAP, NULL);
  if (core == NULL)
  {
    fprintf (stderr, "%s : Can't open ELF (%s)\n", argv[1], elf_errmsg(-1));
    return 3;
  }

  const Dwfl_Callbacks core_callbacks =
  {
    .find_elf = dwfl_build_id_find_elf,
    .find_debuginfo = dwfl_standard_find_debuginfo,
    .section_address = NULL,
    .debuginfo_path = NULL
  };

  Dwfl *dwfl = dwfl_begin (&core_callbacks);
  if (dwfl == NULL)
  {
    fprintf (stderr, "%s : Can't start dwfl (%s)\n", argv[1], dwfl_errmsg(-1));
    return 4;
  }

#if _ELFUTILS_PREREQ(0,158)
  if (dwfl_core_file_report (dwfl, core, NULL) < 0)
#else
  if (dwfl_core_file_report (dwfl, core) < 0)
#endif
  {
    fprintf (stderr, "%s : dwfl report failed (%s)\n", argv[1], dwfl_errmsg(-1));
    return 5;
  }

#if _ELFUTILS_PREREQ(0,158)
  if (dwfl_core_file_attach (dwfl, core) < 0)
  {
    fprintf (stderr, "%s : dwfl attach failed (%s)\n", argv[1], dwfl_errmsg(-1));
    return 6;
  }
#endif

  Regs *regs = get_regs_struct();

  Mappings mappings;
  mappings.elems = 0;

  dwfl_getmodules (dwfl, module_callback, &mappings, 0);

  GElf_Phdr mem;
  GElf_Phdr *phdr = gelf_getphdr (core, 0, &mem);
  if (phdr == NULL || phdr->p_type != PT_NOTE)
  {
    fprintf (stderr, "%s : Missing note section at the first position in core file\n", argv[1]);
    return 8;
  }

  Elf_Data *notes = elf_getdata_rawchunk (core, phdr->p_offset, phdr->p_filesz, ELF_T_NHDR);
  if (notes == NULL)
  {
    fprintf (stderr, "%s : error getting notes (%s)\n", argv[1], dwfl_errmsg(-1));
    return 9;
  }

  Ebl *ebl = ebl_openbackend (core);
  if (ebl == NULL)
  {
    fprintf (stderr, "%s : Can't initialize ebl\n", argv[1]);
    return 7;
  }

  GElf_Nhdr nhdr;
  size_t name_pos;
  size_t desc_pos;
  size_t pos = 0;
  size_t new_pos = 0;
  int got_regs = 0;
  /* registers should be in the first note! */
  while ((new_pos = gelf_getnote (notes, pos, &nhdr, &name_pos, &desc_pos)) > 0)
  {
    if (nhdr.n_type == NT_PRSTATUS && !got_regs)
    {
      GElf_Word regs_offset;
      size_t nregloc;
      const Ebl_Register_Location *reglocs;
      size_t nitems;
      const Ebl_Core_Item *items;

      got_regs = 1;

      if (0 == ebl_core_note (ebl, &nhdr, "CORE", &regs_offset, &nregloc, &reglocs, &nitems, &items))
      {
        fprintf (stderr, "%s : error parsing notes\n", argv[1]);
        return 100;
      }

      const char *regs_location = (const char *)(notes->d_buf) + pos + desc_pos + regs_offset;
      unsigned i;

      for (i = 0; i < nregloc; i++)
      {
        const char *register_location = regs_location + reglocs[i].offset;
        int regnum;
        for (regnum = reglocs[i].regno; regnum < reglocs[i].regno + reglocs[i].count; regnum++)
        {
          char regname[5];
          int bits, type;
          const char *prefix = 0;
          const char *setname = 0;
          ssize_t ret = ebl_register_info (ebl, regnum, regname, sizeof(regname), &prefix, &setname, &bits, &type);
          if (ret < 0)
          {
            fprintf (stderr, "%s : can't get register info\n", argv[1]);
            return 11;
          }
          void *place_for_reg_value = get_place_for_register_value (regname, regnum);

          if (place_for_reg_value != NULL)
            getvalue (core, register_location, bits, place_for_reg_value);

          register_location += bits / 8 + reglocs[i].pad;
        }
      }
    }
    else if (nhdr.n_type == NT_FILE)
    {
      uint64_t values_cnt, page_size;
      const char *values;
      const char *filenames;
      size_t addr_size;

      parse_note_file(core, notes->d_buf + desc_pos, &values_cnt, &page_size, &addr_size, &values, &filenames);
//      printf("addr_size: %d, mappings: %llu, page size: %llu\n", addr_size, values_cnt, page_size);

      int ii;
      /* First: triplets of <mapping-start> <mapping-end> <offset-in-pages>
       *     count = values_cnt
       * Then the names of files.
       */
      for (ii = 0; ii < values_cnt; ii++)
      {
          uint64_t mapping_start, mapping_end, offset_in_pages;
          const char *item = values + 3 * addr_size * ii;

          get_mapping_item (core, addr_size, item, &mapping_start, &mapping_end, &offset_in_pages);
/*          printf ("<0x%08llx-0x%08llx>@0x%08llx: %s\n", mapping_start, mapping_end, offset_in_pages,
              filenames);
*/
          updateMapping (&mappings, mapping_start, mapping_end, offset_in_pages*page_size, filenames);
          filenames += strlen(filenames)+1;
      }
    }
    pos = new_pos;
  }

/*  printf ("PC: 0x%llx\n", (unsigned long long)regs.pc);
  printf ("SP: 0x%llx\n", (unsigned long long)regs.sp);*/

  Callstack callstack;

  create_crash_stack (regs, dwfl, core, &mappings, &callstack);

  char *dem_buffer = NULL;
  size_t it;

  printf ("Call stack:\n");
  for (it = 0; it != callstack.elems; ++it)
  {
    if (sizeof (callstack.tab[0]) > 4)
      printf ("0x%016llx: ", (long long)callstack.tab[it]);
    else
      printf ("0x%08x: ", (int32_t)callstack.tab[it]);
    Dwfl_Module *module = dwfl_addrmodule (dwfl, callstack.tab[it]);
    if (module)
    {
      char *demangled_symbol = 0;
      const char *symbol = dwfl_module_addrname (module, callstack.tab[it]);
      const char *fname = 0;
      const char *module_name = dwfl_module_info (module, NULL, NULL, NULL, NULL, NULL, &fname, NULL);
      char *symbol_from_elf = 0;
      if (symbol == NULL)
      {
        symbol = symbol_from_elf = try_symbol_from_elfs (core, notes, callstack.tab[it], &fname);
      }
      if (symbol != 0 && symbol[0] == '_' && symbol[1] == 'Z')
      {
        int status = -1;

        demangled_symbol = __cxa_demangle (symbol, dem_buffer, NULL, &status);
        if (status == 0)
          symbol = demangled_symbol;
      }
      if (symbol != 0)
        printf ("%s()", symbol);
      else
        printf ("<unknown>");

      if (demangled_symbol != 0)
        free (demangled_symbol);

      if (symbol_from_elf != 0)
        free (symbol_from_elf);

      printf (" from %s\n", fname != NULL ? fname : module_name);
    }
    else
    {
      printf ("unknown function\n");
    }
  }

  dwfl_report_end (dwfl, NULL, NULL);
  dwfl_end (dwfl);
  ebl_closebackend(ebl);
  elf_end (core);
  close (core_fd);

  return 0;
}
