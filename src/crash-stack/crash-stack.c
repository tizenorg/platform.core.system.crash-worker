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

extern char *__cxa_demangle (const char *mangled_name, char *output_buffer,
			     size_t *length, int *status);

/*
static int frame_callback (Dwfl_Frame *state, void *arg)
{
  Regs *regs = static_cast<Regs*>(arg);
  dwfl_frame_pc (state, &regs->pc, NULL);
  return DWARF_CB_ABORT;
}

static int thread_callback (Dwfl_Thread *thread, void *thread_arg)
{
  dwfl_thread_getframes (thread, frame_callback, thread_arg);
  return DWARF_CB_ABORT;
}
*/

static int module_callback (Dwfl_Module *module, void **userdata,
						   const char *name, Dwarf_Addr address,
						   void *arg)
{
  /* To get something from module file do something like:

     GElf_Addr bias;
     Elf *elf = dwfl_module_getelf (module, &bias);

     cout << "Module : " << name << " @" << hex << address << " bias " << bias << endl;

     Elf_Data *data = elf_getdata_rawchunk (elf, 0, 4, ELF_T_BYTE);
     cout << " " << static_cast<char*>(data->d_buf)+1 << endl;
   */
  return DWARF_CB_OK;
}

void getvalue (Elf *core, const void *from, size_t size, void *to)
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

  if (dwfl_core_file_report (dwfl, core, NULL) < 0)
  {
    fprintf (stderr, "%s : dwfl report failed (%s)\n", argv[1], dwfl_errmsg(-1));
    return 5;
  }

  if (dwfl_core_file_attach (dwfl, core) < 0)
  {
    fprintf (stderr, "%s : dwfl attach failed (%s)\n", argv[1], dwfl_errmsg(-1));
    return 6;
  }

   Regs *regs = get_regs_struct();
/*
   To unwind with libelf do this:

  dwfl_getthreads (dwfl, thread_callback, regs);

*/

  dwfl_getmodules (dwfl, module_callback, 0, 0);

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
  /* registers should be in the first note! */
  if (gelf_getnote (notes, pos, &nhdr, &name_pos, &desc_pos) > 0)
  {
    if (nhdr.n_type == NT_PRSTATUS)
    {
      GElf_Word regs_offset;
      size_t nregloc;
      const Ebl_Register_Location *reglocs;
      size_t nitems;
      const Ebl_Core_Item *items;

      if (0 == ebl_core_note (ebl, &nhdr, "CORE", &regs_offset, &nregloc, &reglocs, &nitems, &items))
      {
        fprintf (stderr, "%s : error parsing notes\n", argv[1]);
        return 10;
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
  }

/*  for (int i = 0; i < 20; i++)
  {
    char name[100] = {0};
    int bits = 0, type = 0;
    const char *setname = 0;
    const char *prefix = 0;
    ssize_t ret = ebl_register_info (ebl, i, name, sizeof(name), &prefix, &setname, &bits, &type);
    printf ("ebl_register_info %d ret: %d, name: %s, prefix: %s, setname: %s, bits: %d, type: %d\n",
        i, ret, name, prefix, setname, bits, type);
  }
*/
/*  printf ("PC: 0x%llx\n", (unsigned long long)regs.pc);
  printf ("SP: 0x%llx\n", (unsigned long long)regs.sp);*/

  Callstack callstack;

  create_crash_stack (regs, dwfl, core, &callstack);

  char *dem_buffer = NULL;
  size_t it;

  printf ("Call stack:\n");
  for (it = 0; it != callstack.elems; ++it)
  {
    if (sizeof (callstack.tab[0]) > 4)
      printf ("0x%016llx: ", (int64_t)callstack.tab[it]);
    else
      printf ("0x%08x: ", (int32_t)callstack.tab[it]);
    Dwfl_Module *module = dwfl_addrmodule (dwfl, callstack.tab[it]);
    if (module)
    {
      char *demangled_symbol = 0;
      const char *symbol = dwfl_module_addrname (module, callstack.tab[it]);
      if (symbol != 0 && symbol[0] == '_' && symbol[1] == 'Z')
      {
        int status = -1;

        demangled_symbol = __cxa_demangle (symbol, dem_buffer, NULL, &status);
        if (status == 0)
          symbol = demangled_symbol;
      }
      if (symbol != 0)
        printf ("%s()", symbol);

      if (demangled_symbol != 0)
        free (demangled_symbol);

      printf (" from %s\n", dwfl_module_info (module, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    }
    else
    {
      printf ("unknown function\n");
    }
  }

  dwfl_report_end (dwfl, NULL, NULL);
  dwfl_end (dwfl);
  elf_end (core);
  close (core_fd);

  return 0;
}
