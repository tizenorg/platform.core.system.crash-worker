/*
 * SYS-ASSERT
 * Copyright (c) 2012-2016 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <execinfo.h>
#include <dlfcn.h>
#include <elf.h>
#include <fcntl.h>
#include <ucontext.h>
#include <signal.h>
#include <linux/unistd.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <libunwind.h>
#include "sys-assert.h"
#include "util.h"

#define CMDLINE_PATH "/proc/self/cmdline"
#define EXE_PATH "/proc/self/exe"
#define MAPS_PATH "/proc/self/maps"
#define MEMINFO_PATH "/proc/meminfo"
#define VERINFO_PATH "/etc/info.ini"
#define STATUS_PATH "/proc/self/status"
#define TASK_PATH "/proc/self/task"

#define CRASH_INFO_PATH "/tmp/crash_info"
#define CRASH_SOCKET "/tmp/crash_socket"
#define CRASH_SOCKET_PATH_LEN 17

#define CRASH_CALLSTACKINFO_TITLE "Callstack Information"
#define CRASH_CALLSTACKINFO_TITLE_E "End of Call Stack"
#define CRASH_MAPSINFO_TITLE "Maps Information"
#define CRASH_MAPSINFO_TITLE_E "End of Maps Information"
#define CRASH_MEMINFO_TITLE "Memory Information"
#define CRASH_THREADINFO_TITLE "Threads Information"

#define STR_ANONY "[anony]"
#define STR_ANNOY_LEN 8

#define HEXA 16
#define PERM_LEN 5
#define ADDR_LEN 8
#define INFO_LEN 20
#define VALUE_LEN 24
#define TIME_MAX_LEN 64
#define FILE_LEN 255
#define BUF_SIZE (BUFSIZ)
#define CALLSTACK_SIZE 100
#define FUNC_NAME_MAX_LEN 128
#define PATH_LEN (FILE_LEN + NAME_MAX)

#define KB(bytes)       ((bytes)/1024)

/* permission for open file */
#define DIR_PERMS (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)
/* permission for open file */
#define FILE_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

int sig_to_handle[] = {
	SIGILL, SIGTRAP, SIGABRT, SIGBUS,
	SIGFPE, SIGSEGV, SIGSTKFLT, SIGXCPU, SIGXFSZ, SIGSYS };

#define NUM_SIG_TO_HANDLE	\
	((int)(sizeof(sig_to_handle)/sizeof(sig_to_handle[0])))

struct sigaction g_oldact[NUM_SIG_TO_HANDLE];

extern void dump_registers(int fd, void *context);
extern int dump_callstack(void **callstack_addrs, int size, void *context, int retry);

/* get function symbol from elf */
static int trace_symbols(void *const *array, int size, struct addr_node *start, int fd)
{
	Dl_info info_funcs;
	Elf32_Ehdr elf_h;
	Elf32_Shdr *s_headers;
	Elf32_Sym *symtab_entry;
	int i, cnt, file, ret;
	char *fname;
	unsigned int addr, start_addr, offset_addr;
	unsigned int strtab_index = 0;
	unsigned int symtab_index = 0;
	int num_st = 0;
	int found_symtab = 0;

	for (cnt = 0; cnt < size; cnt++) {
		num_st = 0;
		/* FIXME : for walking on stack trace */
		if (dladdr(array[cnt], &info_funcs) == 0) {
			fprintf(stderr, "[sys-assert]dladdr returnes error!\n");
			/* print just address */
			fprintf_fd(fd,
					"%2d: (%p) %s\n",
					cnt, array[cnt], dlerror());
			continue;
		}
		start_addr = (unsigned int)get_start_addr(array[cnt], start);
		addr = (unsigned int)array[cnt];
		/* because of launchpad,
		 * return value of dladdr when find executable is wrong.
		 * so fix dli_fname here */
		if (info_funcs.dli_fbase &&
			info_funcs.dli_fname &&
			info_funcs.dli_fbase == (void *)BASE_LAUNCHPAD_ADDR &&
			(strncmp("/opt/apps/", info_funcs.dli_fname,
					 strlen("/opt/apps/")) == 0)) {
			info_funcs.dli_fname = get_fpath(array[cnt], start);
			offset_addr = addr;
		} else {
			offset_addr = addr - start_addr;
		}

		/* find symbol from elf file */
		if (info_funcs.dli_sname == NULL) {

			/* Both dli_sname and dli_fname is NULL, sys-assert cannot trace any information.
			   Thus, sys-assert skips to translate such address entry.
			   However, if a developer wants raw information, we need to fix the code to print raw data */
			if(info_funcs.dli_fname == NULL)
				continue;

			file = open(info_funcs.dli_fname, O_RDONLY);
			if (file < 0) {
				fname = strchr(info_funcs.dli_fname, '/');
				if (!fname)
					continue;
				file = open(fname, O_RDONLY);
				if (file < 0) {
					fprintf_fd(fd,
							"%2d: (%p) [%s] + %p\n",
							cnt, array[cnt],
							info_funcs.dli_fname, offset_addr);
					continue;
				}
			}
			ret = read(file, &elf_h, sizeof(Elf32_Ehdr));
			if (ret < sizeof(Elf32_Ehdr) ||
					elf_h.e_shnum <= 0 ||
					SHN_LORESERVE < elf_h.e_shnum) {
				fprintf_fd(fd, "%2d: (%p) [%s] + %p\n",
						cnt, array[cnt], info_funcs.dli_fname, offset_addr);
				close(file);
				continue;
			}
			if (elf_h.e_type == ET_EXEC) {
				info_funcs.dli_fbase = 0;
				offset_addr = addr;
			}
			s_headers =
				(Elf32_Shdr *) mmap(0, elf_h.e_shnum * sizeof(Elf32_Shdr),
						PROT_READ | PROT_WRITE,
						MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			if (s_headers == NULL) {
				fprintf(stderr, "[sys-assert]malloc failed\n");
				fprintf_fd(fd, "%2d: (%p) [%s] + %p\n",
						cnt, array[cnt], info_funcs.dli_fname, offset_addr);
				close(file);
				continue;
			}
			ret = lseek(file, elf_h.e_shoff, SEEK_SET);
			if (ret < 0 || elf_h.e_shentsize > sizeof(Elf32_Shdr) ||
					elf_h.e_shentsize <= 0) {
				close(file);
				munmap(s_headers, elf_h.e_shnum * sizeof(Elf32_Shdr));
				return -1;
			}
			for (i = 0; i < elf_h.e_shnum; i++) {
				ret = read(file, &s_headers[i], elf_h.e_shentsize);
				if (ret < elf_h.e_shentsize) {
					fprintf(stderr,	"[sys-assert]read error\n");
					munmap(s_headers, elf_h.e_shnum * sizeof(Elf32_Shdr));
					close(file);
					return -1;
				}
			}
			for (i = 0; i < elf_h.e_shnum; i++) {
				if (s_headers[i].sh_type == SHT_SYMTAB) {
					symtab_index = i;
					if (s_headers[i].sh_entsize != 0 &&
							s_headers[i].sh_size != 0) {
						num_st =
							s_headers[i].sh_size / s_headers[i].sh_entsize;
						found_symtab = 1;
					}
					break;
				}
			}
			if (!found_symtab) {
				fprintf(stderr,
						"[sys-assert] can't find symtab\n");
				munmap(s_headers, elf_h.e_shnum * sizeof(Elf32_Shdr));
				close(file);
			} else {
				/*.strtab index */
				if (symtab_index < elf_h.e_shnum)
					strtab_index = s_headers[symtab_index].sh_link;
				if (!strtab_index ||  elf_h.e_shnum <= strtab_index) {
					fprintf_fd(fd, "%2d: (%p) [%s] + %p\n",
						cnt, array[cnt], info_funcs.dli_fname, offset_addr);
					munmap(s_headers, elf_h.e_shnum * sizeof(Elf32_Shdr));
					close(file);
					continue;
				}
				symtab_entry =
					(Elf32_Sym *)mmap(0, sizeof(Elf32_Sym) * num_st,
						PROT_READ | PROT_WRITE,
						MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
				if (symtab_entry == NULL) {
					fprintf(stderr, "[sys-assert]malloc failed\n");
					munmap(s_headers, elf_h.e_shnum * sizeof(Elf32_Shdr));
					close(file);
					return -1;
				}
				ret = lseek(file, s_headers[symtab_index].sh_offset, SEEK_SET);
				if (ret < 0) {
					fprintf_fd(fd, "%2d: (%p) [%s] + %p\n",
						cnt, array[cnt], info_funcs.dli_fname, offset_addr);
					munmap(symtab_entry, sizeof(Elf32_Sym) * num_st);
					munmap(s_headers, elf_h.e_shnum * sizeof(Elf32_Shdr));
					close(file);
					continue;
				}
				for (i = 0; i < num_st; i++) {
					ret = read(file, &symtab_entry[i], sizeof(Elf32_Sym));
					if (ret < sizeof(Elf32_Sym)) {
						fprintf_fd(fd,
							"[sys-assert]symtab_entry[%d], num_st=%d, readnum = %d\n",
								i, num_st, ret);
						break;
					}
					if (((info_funcs.dli_fbase +
									symtab_entry[i].st_value)
								<= array[cnt])
							&& (array[cnt] <=
								(info_funcs.dli_fbase +
								 symtab_entry[i].st_value +
								 symtab_entry[i].st_size))) {
						if (symtab_entry[i].st_shndx != STN_UNDEF) {
							ret = lseek(file,
									s_headers[strtab_index].sh_offset +
									symtab_entry[i].st_name,
									SEEK_SET);
							if (ret < 0)
								break;
							info_funcs.dli_sname =
								(void *)mmap(0, FUNC_NAME_MAX_LEN,
									PROT_READ | PROT_WRITE,
									MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
							ret = read(file, (void *)info_funcs.dli_sname,
									FUNC_NAME_MAX_LEN);
							if (ret < 0)
								break;
							info_funcs.dli_saddr =
								info_funcs.dli_fbase +
								symtab_entry[i].st_value;
						}
						break;
					}
				}
				munmap(s_headers, elf_h.e_shnum * sizeof(Elf32_Shdr));
				munmap(symtab_entry, sizeof(Elf32_Sym) * num_st);
				close(file);
			}
		}
		/* print symbol name and address. */
		if (info_funcs.dli_sname != NULL) {
			if (array[cnt] >= info_funcs.dli_saddr)
				fprintf_fd(fd, "%2d: %s + 0x%x (%p) [%s] + %p\n",
						cnt, info_funcs.dli_sname,
						(array[cnt] - info_funcs.dli_saddr),
						array[cnt], info_funcs.dli_fname, offset_addr);
			else
				fprintf_fd(fd, "%2d: %s - 0x%x (%p) [%s] + %p\n",
						cnt, info_funcs.dli_sname,
						(info_funcs.dli_saddr - array[cnt]),
						array[cnt], info_funcs.dli_fname, offset_addr);
		} else {
			fprintf_fd(fd, "%2d: (%p) [%s] + %p\n",
					cnt, array[cnt], info_funcs.dli_fname, offset_addr);
		}
	}
	return 0;
}
/* get address list from maps */
static struct addr_node *get_addr_list_from_maps(int fd)
{
	int fpath_len, result;
	long *saddr;
	long *eaddr;
	char perm[PERM_LEN];
	char path[PATH_LEN];
	char addr[ADDR_LEN * 2];
	char linebuf[BUF_SIZE];
	struct addr_node *head = NULL;
	struct addr_node *tail = NULL;
	struct addr_node *t_node = NULL;

	/* parsing the maps to get executable code address */
	while (fgets_fd(linebuf, BUF_SIZE, fd) != NULL) {
		memset(path, 0, PATH_LEN);
		result = sscanf(linebuf, "%s %s %*s %*s %*s %s ", addr, perm, path);
		if (result < 0)
			continue;
		perm[PERM_LEN - 1] = 0;
		/* rwxp */
#ifdef ARM
		if ((perm[2] == 'x' && path[0] == '/') ||
				(perm[1] == 'w' && path[0] != '/')) {
#else
		if (strncmp(perm, "r-xp", strlen("r-xp")) == 0) {
#endif
			/* add addr node to list */
			addr[ADDR_LEN] = 0;
			saddr = (long *)strtoul(addr, NULL, HEXA);
			/* ffff0000-ffff1000 */
			eaddr = (long *)strtoul(&addr[ADDR_LEN + 1], NULL, HEXA);
			/* make node and attach to the list */
			t_node = (struct addr_node *)mmap(0, sizeof(struct addr_node),
					PROT_READ | PROT_WRITE,
					MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			if (t_node == NULL) {
				fprintf(stderr, "error : mmap\n");
				return NULL;
			}
			memcpy(t_node->perm, perm, PERM_LEN);
			t_node->startaddr = saddr;
			t_node->endaddr = eaddr;
			t_node->fpath = NULL;
			fpath_len = strlen(path);
			if (fpath_len > 0) {
				t_node->fpath = (char *)mmap(0, fpath_len + 1,
						PROT_READ | PROT_WRITE,
						MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
				memset(t_node->fpath, 0, fpath_len + 1);
				memcpy(t_node->fpath, path, fpath_len);
			} else {
				t_node->fpath = (char *)mmap(0, STR_ANNOY_LEN,
						PROT_READ | PROT_WRITE,
						MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
				memset(t_node->fpath, 0, STR_ANNOY_LEN);
				memcpy(t_node->fpath, STR_ANONY, STR_ANNOY_LEN);
			}
			t_node->next = NULL;
			if (head == NULL) {
				head = t_node;
				tail = t_node;
			} else {
				tail->next = t_node;
				tail = t_node;
			}
		}
	}
	return head;
}

static void print_node_to_file(struct addr_node *start, int fd)
{
	struct addr_node *t_node;

	t_node = start;
	fprintf_fd(fd, "\n%s\n", CRASH_MAPSINFO_TITLE);
	while (t_node) {
		if (!strncmp(STR_ANONY, t_node->fpath, STR_ANNOY_LEN)) {
			t_node = t_node->next;
		} else {
			fprintf_fd(fd,
					"%08x %08x %s %s\n",
					(unsigned int)t_node->startaddr,
					(unsigned int)t_node->endaddr,
					t_node->perm, t_node->fpath);
			t_node = t_node->next;
		}
	}
	fprintf_fd(fd, "%s\n", CRASH_MAPSINFO_TITLE_E);
}

static void free_all_nodes(struct addr_node *start)
{
	struct addr_node *t_node, *n_node;
	int fpath_len;

	if (start == NULL)
		return;
	t_node = start;
	n_node = t_node->next;
	while (t_node) {
		if (t_node->fpath != NULL) {
			fpath_len = strlen(t_node->fpath);
			munmap(t_node->fpath, fpath_len + 1);
		}
		munmap(t_node, sizeof(struct addr_node));
		if (n_node == NULL)
			break;
		t_node = n_node;
		n_node = n_node->next;
	}
}

static void print_signal_info(int signum, const siginfo_t *info, int fd)
{
	fprintf_fd(fd, "Signal: %d\n", signum);
	switch (signum) {
	case SIGILL:
		fprintf_fd(fd, "      (SIGILL)\n");
		break;
	case SIGTRAP:
		fprintf_fd(fd, "      (SIGTRAP)\n");
		break;
	case SIGABRT:
		fprintf_fd(fd, "      (SIGABRT)\n");
		break;
	case SIGBUS:
		fprintf_fd(fd, "      (SIGBUS)\n");
		break;
	case SIGFPE:
		fprintf_fd(fd, "      (SIGFPE)\n");
		break;
	case SIGSEGV:
		fprintf_fd(fd, "      (SIGSEGV)\n");
		break;
	case SIGTERM:
		fprintf_fd(fd, "      (SIGTERM)\n");
		break;
	case SIGSTKFLT:
		fprintf_fd(fd, "      (SIGSTKFLT)\n");
		break;
	case SIGXCPU:
		fprintf_fd(fd, "      (SIGXCPU)\n");
		break;
	case SIGXFSZ:
		fprintf_fd(fd, "      (SIGXFSZ)\n");
		break;
	case SIGSYS:
		fprintf_fd(fd, "      (SIGSYS)\n");
		break;
	default:
		fprintf_fd(fd, "\n");
	}
	/* print signal si_code info */
	fprintf_fd(fd, "      si_code: %d\n", info->si_code);
	if (info->si_code <= 0 || info->si_code >= 0x80) {
		switch (info->si_code) {
#ifdef SI_TKILL
		case SI_TKILL:
			fprintf_fd(fd,
					"      signal sent by tkill (sent by pid %d, uid %d)\n",
				info->si_pid, info->si_uid);
			break;
#endif
#ifdef SI_USER
		case SI_USER:
			fprintf_fd(fd,
				"      signal sent by kill (sent by pid %d, uid %d)\n",
				info->si_pid, info->si_uid);
			break;
#endif
#ifdef SI_KERNEL
		case SI_KERNEL:
			fprintf_fd(fd, "      signal sent by the kernel\n");
			break;
#endif
		}
	} else if (signum == SIGILL) {
		switch (info->si_code) {
		case ILL_ILLOPC:
			fprintf_fd(fd, "      illegal opcode\n");
			break;
		case ILL_ILLOPN:
			fprintf_fd(fd, "      illegal operand\n");
			break;
		case ILL_ILLADR:
			fprintf_fd(fd, "      illegal addressing mode\n");
			break;
		case ILL_ILLTRP:
			fprintf_fd(fd, "      illegal trap\n");
			break;
		case ILL_PRVOPC:
			fprintf_fd(fd, "      privileged opcode\n");
			break;
		case ILL_PRVREG:
			fprintf_fd(fd, "      privileged register\n");
			break;
		case ILL_COPROC:
			fprintf_fd(fd, "      coprocessor error\n");
			break;
		case ILL_BADSTK:
			fprintf_fd(fd, "      internal stack error\n");
			break;
		default:
			fprintf_fd(fd, "      illegal si_code = %d\n", info->si_code);
			break;
		}
		fprintf_fd(fd, "      si_addr: %p\n", info->si_addr);
	} else if (signum == SIGFPE) {
		switch (info->si_code) {
		case FPE_INTDIV:
			fprintf_fd(fd, "      integer divide by zero\n");
			break;
		case FPE_INTOVF:
			fprintf_fd(fd, "      integer overflow\n");
			break;
		case FPE_FLTDIV:
			fprintf_fd(fd, "      floating-point divide by zero\n");
			break;
		case FPE_FLTOVF:
			fprintf_fd(fd, "      floating-point overflow\n");
			break;
		case FPE_FLTUND:
			fprintf_fd(fd, "      floating-point underflow\n");
			break;
		case FPE_FLTRES:
			fprintf_fd(fd, "      floating-point inexact result\n");
			break;
		case FPE_FLTINV:
			fprintf_fd(fd, "      invalid floating-point operation\n");
			break;
		case FPE_FLTSUB:
			fprintf_fd(fd, "      subscript out of range\n");
			break;
		default:
			fprintf_fd(fd, "      illegal si_code: %d\n", info->si_code);
			break;
		}
	} else if (signum == SIGSEGV) {
		switch (info->si_code) {
			case SEGV_MAPERR:
				fprintf_fd(fd, "      address not mapped to object\n");
				break;
			case SEGV_ACCERR:
				fprintf_fd(fd,
						"      invalid permissions for mapped object\n");
				break;
			default:
				fprintf_fd(fd, "      illegal si_code: %d\n", info->si_code);
				break;
		}
		fprintf_fd(fd, "      si_addr = %p\n", info->si_addr);
	} else if (signum == SIGBUS) {
		switch (info->si_code) {
			case BUS_ADRALN:
				fprintf_fd(fd, "      invalid address alignment\n");
				break;
			case BUS_ADRERR:
				fprintf_fd(fd, "      nonexistent physical address\n");
				break;
			case BUS_OBJERR:
				fprintf_fd(fd, "      object-specific hardware error\n");
				break;
			default:
				fprintf_fd(fd, "      illegal si_code: %d\n", info->si_code);
				break;
		}
		fprintf_fd(fd, "      si_addr: %p\n", info->si_addr);
	}
}

void sighandler(int signum, siginfo_t *info, void *context)
{
	int idx;
	int readnum;
	int threadnum;
	/* file descriptor */
	int fd;
	int fd_cs;		/* for cs file */
	pid_t pid;
	pid_t tid;
	DIR *dir;
	struct dirent *dentry;
	char timestr[TIME_MAX_LEN];
	char processname[NAME_MAX] = {0,};
	char exepath[PATH_LEN] = {0,};
	char filepath[PATH_LEN];
	char crashid[TIME_MAX_LEN] = {0,};
	/* for get time  */
	time_t cur_time;
	/* for get info */
	char infoname[INFO_LEN];
	char value[VALUE_LEN];
	char linebuf[BUF_SIZE];
	char *p_exepath = NULL;
	void *callstack_addrs[CALLSTACK_SIZE];
	int cnt_callstack = 0;
	/* for backtrace_symbols() */
	struct addr_node *head = NULL;
	/* for preventing recursion */
	static int retry_count = 0;
	struct sysinfo si;
	/* for notification */
	int sent, sockfd = -1;
	struct timeval tv_timeo = { 3, 500000 };
	struct sockaddr_un clientaddr;

	if (retry_count > 1) {
		fprintf(stderr, "[sys-assert] recurcive called\n");
		return;
	}

	cur_time = time(NULL);
	/* get pid */
	pid = getpid();
	tid = (long int)syscall(__NR_gettid);
	/* open maps file */
	if ((fd = open(MAPS_PATH, O_RDONLY)) < 0) {
		fprintf(stderr, "[sys-assert]can't open %s\n", MAPS_PATH);
	} else {
		/* parsing the maps to get code segment address*/
		head = get_addr_list_from_maps(fd);
		close(fd);
	}
	if (retry_count)
		fprintf(stderr, "retry backtrace in sighandler");
	cnt_callstack = dump_callstack(callstack_addrs, CALLSTACK_SIZE, context, retry_count);
	retry_count += 1;
	/* get exepath */
	if ((readnum = open_read(CMDLINE_PATH, exepath, sizeof(exepath) - 1)) <= 0) {
		fprintf(stderr, "[sys-assert]can't read %s\n", CMDLINE_PATH);
		readnum = snprintf(exepath, sizeof(exepath), "unknown_process");
	}
	exepath[readnum] = '\0';
	/* get processname */
	if ((p_exepath = remove_path(exepath)) == NULL)
		return;
	snprintf(processname, NAME_MAX, "%s", p_exepath);
	/* added temporary skip  when crash-worker is asserted */
	if (!strcmp(processname, "crash-worker") ||
			!strcmp(processname, "crash-popup"))
		return;
	/* make crash info file name */
	snprintf(timestr, sizeof(timestr), "%.10ld", cur_time);
	snprintf(crashid, sizeof(crashid), "%.2d%.5d%02x%02x%02x%s",
				signum, pid, processname[0], processname[1], processname[2], timestr);
	if (snprintf(filepath, PATH_LEN,
				"%s/%s.info", CRASH_INFO_PATH, crashid) == 0) {
		fprintf(stderr,
				"[sys-assert]can't make crash info file name : %s\n",
				crashid);
		return;
	}
	/* check crash info dump directory, make directory if absent */
	if (access(CRASH_INFO_PATH, F_OK) == -1) {
		fprintf(stderr, "[sys-assert] No directory (%s)", CRASH_INFO_PATH);
		/*TODO: making directory */
		return;
	}
	/* logging crash information to stderr */
	fprintf(stderr, "crashed [%s] processname=%s, pid=%d, tid=%d, signal=%d",
			timestr, processname, pid, tid, signum);
	/* complete filepath_cs */
	if (!strlen(filepath))
		return;
	/* create cs file */
	if ((fd_cs = creat(filepath, FILE_PERMS)) < 0) {
		fprintf(stderr,
				"[sys-assert]can't create %s. errno = %s\n",
				filepath, strerror(errno));
		return;
	}

	/* print exepath info */
	fprintf_fd(fd_cs, "Executable File Path: %s\n", exepath);

	/* print signal info */
	print_signal_info(signum, info, fd_cs);
	fsync(fd_cs);
	/* print register info */
	dump_registers(fd_cs, context);

	/* print meminfo */
	fprintf_fd(fd_cs, "\n%s\n", CRASH_MEMINFO_TITLE);
	 if (!sysinfo(&si)) {
		 fprintf_fd(fd_cs, "MemTotal: %8ld KB\n", KB(si.totalram));
		 fprintf_fd(fd_cs, "MemFree:  %8ld KB\n", KB(si.freeram));
		 fprintf_fd(fd_cs, "Buffers:  %8ld KB\n", KB(si.bufferram));
	} else
		fprintf(stderr, "[sys-assert]can't get sysinfo\n");

	if ((fd = open(MEMINFO_PATH, O_RDONLY)) < 0) {
		fprintf(stderr, "[sys-assert]can't open %s\n", MEMINFO_PATH);
	} else {
		while (fgets_fd(linebuf, BUF_SIZE, fd) != NULL) {
			sscanf(linebuf, "%s %s %*s", infoname, value);
			if (strcmp("Cached:", infoname) == 0) {
				fprintf_fd(fd_cs, "%s   %8s KB\n", infoname, value);
				break;
			}
		}
		close(fd);
	}

	threadnum = 0;
	if ((fd = open(STATUS_PATH, O_RDONLY)) < 0) {
		fprintf(stderr, "[sys-assert]can't open %s\n", STATUS_PATH);
	} else {
		while (fgets_fd(linebuf, BUF_SIZE, fd) != NULL) {
			sscanf(linebuf, "%s %s %*s", infoname, value);
			if (strcmp("VmPeak:", infoname) == 0) {
				fprintf_fd(fd_cs, "%s   %8s KB\n", infoname,
						value);
			} else if (strcmp("VmSize:", infoname) == 0) {
				fprintf_fd(fd_cs, "%s   %8s KB\n", infoname,
						value);
			} else if (strcmp("VmLck:", infoname) == 0) {
				fprintf_fd(fd_cs, "%s    %8s KB\n", infoname,
						value);
			} else if (strcmp("VmPin:", infoname) == 0) {
				fprintf_fd(fd_cs, "%s    %8s KB\n", infoname,
						value);
			} else if (strcmp("VmHWM:", infoname) == 0) {
				fprintf_fd(fd_cs, "%s    %8s KB\n",
						infoname, value);
			} else if (strcmp("VmRSS:", infoname) == 0) {
				fprintf_fd(fd_cs, "%s    %8s KB\n",
						infoname, value);
			} else if (strcmp("VmData:", infoname) == 0) {
				fprintf_fd(fd_cs, "%s   %8s KB\n",
						infoname, value);
			} else if (strcmp("VmStk:", infoname) == 0) {
				fprintf_fd(fd_cs, "%s    %8s KB\n",
						infoname, value);
			} else if (strcmp("VmExe:", infoname) == 0) {
				fprintf_fd(fd_cs, "%s    %8s KB\n",
						infoname, value);
			} else if (strcmp("VmLib:", infoname) == 0) {
				fprintf_fd(fd_cs, "%s    %8s KB\n",
						infoname, value);
			} else if (strcmp("VmPTE:", infoname) == 0) {
				fprintf_fd(fd_cs, "%s    %8s KB\n",
						infoname, value);
			} else if (strcmp("VmSwap:", infoname) == 0) {
				fprintf_fd(fd_cs, "%s   %8s KB\n",
						infoname, value);
			} else if (strcmp("Threads:", infoname) == 0) {
				threadnum = atoi(value);
				break;
			}
		}
		close(fd);
	}
	/* print thread info */
	if (1 < threadnum) {
		fprintf_fd(fd_cs, "\n%s\n", CRASH_THREADINFO_TITLE);
		fprintf_fd(fd_cs,
				"Threads: %d\nPID = %d TID = %d\n",
				threadnum, pid, tid);
		/* print thread */
		dir = opendir(TASK_PATH);
		if (!dir) {
			fprintf(stderr, "[sys-assert]can't open %s\n", TASK_PATH);
		} else {
			while ((dentry = readdir(dir)) != NULL) {
				if( strcmp(dentry->d_name, ".") == 0
						|| strcmp(dentry->d_name, "..") == 0)
					continue;
				fprintf_fd(fd_cs, "%s ",dentry->d_name);
			}
			closedir(dir);
			fprintf_fd(fd_cs, "\n");
		}
	}
	if (head != NULL) {
		/* print maps information */
		print_node_to_file(head, fd_cs);
		/* print callstack */
		fprintf_fd(fd_cs, "\n%s (PID:%d)\n", CRASH_CALLSTACKINFO_TITLE, pid);
		fprintf_fd(fd_cs, "Call Stack Count: %d\n", cnt_callstack);
		if (trace_symbols(&callstack_addrs[CALLSTACK_BASE],
					cnt_callstack, head, fd_cs) < 0)
			fprintf(stderr, "[sys-assert] trace_symbols failed\n");
		fprintf_fd(fd_cs, "%s\n", CRASH_CALLSTACKINFO_TITLE_E);
		free_all_nodes(head);
	}
	/* cs file sync */
	fsync(fd_cs);
	/* clean up */
	if (close(fd_cs) == -1)
		fprintf(stderr, "[sys-assert] fd_cs close error!!\n");
	/* core dump set */
#ifdef TIZEN_ENABLE_COREDUMP
	if (prctl(PR_GET_DUMPABLE) == 0)
		prctl(PR_SET_DUMPABLE, 1);
#else
	prctl(PR_SET_DUMPABLE, 0);
#endif
	/* NOTIFY CRASH */
	if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "[sys-assert] failed socket()");
		goto exit;
	}
	if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO,
				&tv_timeo, sizeof(tv_timeo) ) < 0) {
		fprintf(stderr, "[sys-assert] setsockopt SO_SNDTIMEO");
		close(sockfd);
		goto exit;
	}
	bzero(&clientaddr, sizeof(clientaddr));
	clientaddr.sun_family = AF_UNIX;
	memset(clientaddr.sun_path, 0x00, sizeof(clientaddr.sun_path));
	strncpy(clientaddr.sun_path, CRASH_SOCKET, CRASH_SOCKET_PATH_LEN);
	clientaddr.sun_path[CRASH_SOCKET_PATH_LEN] = '\0';

	if (connect(sockfd, (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0) {
		close(sockfd);
		fprintf(stderr, "[sys-assert] failed connect()");
		goto exit;
	}
	snprintf(linebuf, sizeof(linebuf),
			"%d|%d|%s|%s|%s|%s", signum, pid, timestr,
			processname, exepath, crashid);

	sent = write(sockfd, linebuf, strlen(linebuf));
	if (sent < 0)
		fprintf(stderr, "[sys-assert] failed write()");
	close(sockfd);
exit:
	for (idx = 0; idx < NUM_SIG_TO_HANDLE; idx++) {
		if (sig_to_handle[idx] == signum) {
			sigaction(signum, &g_oldact[idx], NULL);
			break;
		}
	}
	raise(signum);
}

__attribute__ ((constructor))
void init()
{
	int idx;

	for (idx = 0; idx < NUM_SIG_TO_HANDLE; idx++) {
		struct sigaction act;
		act.sa_sigaction = (void *)sighandler;
		sigemptyset(&act.sa_mask);
		act.sa_flags = SA_SIGINFO;
		act.sa_flags |= SA_RESETHAND;
		if (sigaction(sig_to_handle[idx], &act, &g_oldact[idx]) < 0) {
			perror("[sys-assert]could not set signal handler ");
			continue;
		}
	}
}
