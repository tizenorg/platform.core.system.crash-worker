#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <vector>
#include <libelf.h>
#include <elfutils/libdwfl.h>
#include <elfutils/libebl.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <cxxabi.h>

extern "C" {
#include "wind/unwarm.h"
}

using namespace std;

struct Regs
{
  Dwarf_Addr regs[16];
  Dwarf_Addr sp;
  Dwarf_Addr lr;
  Dwarf_Addr pc;
  Dwarf_Addr spsr;
};

typedef vector<Int32> Callstack;

static Boolean report (void *data, Int32 address)
{
  Callstack *callstack = static_cast<Callstack *>(data);
  callstack->push_back (address);

  return TRUE;
}

Dwfl *g_dwfl = 0;
Elf *g_core = 0;

namespace {
template <class T>
Boolean readT (Int32 a, T *v)
{
  Dwfl_Module *module = 0;
  
  int segment = dwfl_addrsegment (g_dwfl, a, &module);

  if (module != NULL)
  {
    Dwarf_Addr start;
    dwfl_module_info (module, NULL, &start, NULL, NULL, NULL, NULL, NULL);

    GElf_Addr bias;
    Elf *elf = dwfl_module_getelf (module, &bias);

    Elf_Data *data = elf_getdata_rawchunk (elf, a-start, sizeof(T), ELF_T_BYTE);
    *v = *(T*)(data->d_buf);
//    cerr << "get data from 0x" << hex << a;
//    cerr << " = " << hex << *v << endl;
  }
  else if (segment != -1)
  {
    // get data from segment
//    cerr << "get data from 0x" << hex << a;
    GElf_Phdr mem;
    GElf_Phdr *phdr = gelf_getphdr (g_core, segment, &mem);
    Dwarf_Addr offset_in_segment = a - phdr->p_vaddr;
    Dwarf_Addr offset_in_file = phdr->p_offset + offset_in_segment;

    Elf_Data *data = elf_getdata_rawchunk (g_core, offset_in_file, sizeof(T), ELF_T_BYTE);
    *v = *(T*)(data->d_buf);
//    cerr << " = " << hex << *v << endl;
  }
  else
  {
//    cerr << "Can't read from address 0x" << hex << a << endl;
    return FALSE; // can't unwind further - no data to lookup
  }

  return TRUE;
}
}

static Boolean readW (Int32 a, Int32 *v)
{
  return readT(a,v);
}

static Boolean readH (Int32 a, Int16 *v)
{
  return readT(a,v);
}

static Boolean readB (Int32 a, Int8 *v)
{
  return readT(a,v);
}
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
//  GElf_Addr bias;
//  Elf *elf = dwfl_module_getelf (module, &bias);

//  cout << "Module : " << name << " @" << hex << address << " bias " << bias << endl;
/*  Elf_Data *data = elf_getdata_rawchunk (elf, 0, 4, ELF_T_BYTE);
  cout << " " << static_cast<char*>(data->d_buf)+1 << endl;*/
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
    .d_buf = const_cast<void*>(from),
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
    cerr << "failed to get value from core file" << endl;
}

int main(int argc, char **argv)
{
  if (argc != 2)
  {
    cerr << "Usage: " << argv[0] << " <core-file>" << endl;
    return 1;
  }

  UnwindCallbacks callbacks =
  {
    report,
    readW,
    readH,
    readB
#ifdef UNW_DEBUG
    ,
    printf
#endif
  };

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
    cerr << argv[1] << " : Can't open ELF (" << elf_errmsg(-1) << ")" << endl;
    return 3;
  }

  g_core = core;

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
    cerr << argv[1] << " : Can't start dwfl (" << dwfl_errmsg(-1) << ")" << endl;
    return 4;
  }
  g_dwfl = dwfl;

  if (dwfl_core_file_report (dwfl, core, NULL) < 0)
  {
    cerr << argv[1] << " : dwfl report failed (" << dwfl_errmsg(-1) << ")" << endl;
    return 5;
  }

  if (dwfl_core_file_attach (dwfl, core) < 0)
  {
    cerr << argv[1] << " : dwfl attach failed (" << dwfl_errmsg(-1) << ")" << endl;
    return 6;
  }

   Regs regs = {0};
/*
  dwfl_getthreads (dwfl, thread_callback, &regs);

  dwfl_report_end (dwfl, NULL, NULL);
*/

  dwfl_getmodules (dwfl, module_callback, 0, 0);

  GElf_Phdr mem;
  GElf_Phdr *phdr = gelf_getphdr (core, 0, &mem);
  if (phdr == NULL || phdr->p_type != PT_NOTE)
  {
    cerr << argv[1] << " : Missing note section at the first position in core file" << endl;
    return 8;
  }

  Elf_Data *notes = elf_getdata_rawchunk (core, phdr->p_offset, phdr->p_filesz, ELF_T_NHDR);
  if (notes == NULL)
  {
    cerr << argv[1] << " : error getting notes (" << elf_errmsg(-1) << ")" << endl;
    return 9;
  }

  Ebl *ebl = ebl_openbackend (core);
  if (ebl == NULL)
  {
    cerr << argv[1] << " : Can't initialize ebl" << endl;
    return 7;
  }

  GElf_Nhdr nhdr;
  size_t name_pos;
  size_t desc_pos;
  size_t pos = 0;
  size_t new_pos = 0;
  while ((new_pos = gelf_getnote (notes, pos, &nhdr, &name_pos, &desc_pos)) > 0)
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
        cerr << argv[1] << " : error parsing notes" << endl;
        return 10;
      }

      const char *regs_location = static_cast<const char *>(notes->d_buf) + pos + desc_pos + regs_offset;

      for (unsigned i = 0; i < nregloc; i++)
      {
        const char *register_location = regs_location + reglocs[i].offset;
        for (int regnum = reglocs[i].regno; regnum < reglocs[i].regno + reglocs[i].count; regnum++)
        {
          char regname[5];
          int bits, type;
          const char *prefix = 0;
          const char *setname = 0;
          ssize_t ret = ebl_register_info (ebl, regnum, regname, sizeof(regname), &prefix, &setname, &bits, &type);
          if (ret < 0)
          {
            cerr << " : can't get register info" << endl;
            return 11;
          }
          string s = regname;
          if (s == "pc")
          {
            getvalue (core, register_location, bits, &regs.pc);
            regs.regs[15] = regs.pc;
          }
          else if (s == "sp")
          {
            getvalue (core, register_location, bits, &regs.sp);
            regs.regs[13] = regs.sp;
          }
          else if (s == "lr")
          {
            getvalue (core, register_location, bits, &regs.lr);
          }
          else if (s == "spsr")
          {
            getvalue (core, register_location, bits, &regs.spsr);
          }
          else if (regnum < 16)
          {
            getvalue (core, register_location, bits, &regs.regs[regnum]);
          }
          register_location += bits / 8 + reglocs[i].pad;
        }
      }
    }
    pos = new_pos;
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
  cout << "PC: 0x" << hex << regs.pc << endl;
  cout << "SP: 0x" << hex << regs.sp << endl;

  UnwState state;

  Callstack callstack;
  callstack.push_back (regs.pc);

  UnwInitState (&state, &callbacks, &callstack, regs.pc, regs.sp);
  for (int i = 0; i < 13; i++)
  {
    state.regData[i].v = regs.regs[i];
    state.regData[i].o = REG_VAL_FROM_CONST;
  }
  state.regData[REG_LR].v = regs.lr;
  state.regData[REG_LR].o = REG_VAL_FROM_STACK;
  state.regData[REG_SPSR].v = regs.spsr;
  state.regData[REG_SPSR].o = REG_VAL_FROM_CONST;

  if (UnwIsAddrThumb (regs.pc, regs.spsr))
    UnwStartThumb (&state);
  else
    UnwStartArm (&state);

  size_t dem_buffer_len = 0;
  char *dem_buffer = NULL;

  cout << "Call stack:" << endl;
  for (Callstack::iterator it = callstack.begin(); it != callstack.end(); ++it)
  {
    cout << "0x" << hex << *it << ": ";
    Dwfl_Module *module = dwfl_addrmodule (dwfl, *it);
    if (module)
    {
      const char *symbol = dwfl_module_addrname (module, *it);
      if (symbol != 0 && symbol[0] == '_' && symbol[1] == 'Z')
      {
        int status = -1;

        char *demangled_symbol = abi::__cxa_demangle (symbol, dem_buffer, &dem_buffer_len, &status);
        if (status == 0)
          symbol = demangled_symbol;
      }
      if (symbol != 0)
        cout << symbol << "() ";
      cout << "from " << dwfl_module_info (module, NULL, NULL, NULL, NULL, NULL, NULL, NULL) << endl;
    }
    else
    {
      cout << "unknown function" << endl;
    }
  }

  dwfl_end (dwfl);
  elf_end (core);
  close (core_fd);

  return 0;
}
