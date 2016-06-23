/*
 * SYS-ASSERT
 * Copyright (c) 2012-2013 Samsung Electronics Co., Ltd.
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
#include <ucontext.h>
#include "sys-assert.h"
#include "util.h"

#define CRASH_REGISTERINFO_TITLE "Register Information"

void dump_registers(int fd, void *context)
{
	ucontext_t *ucontext = context;
	/* for context info */
	if (!context)
		return;

	fprintf_fd(fd, "\n%s\n", CRASH_REGISTERINFO_TITLE);
	fprintf_fd(fd,
		"gs  = 0x%08x, fs  = 0x%08x\nes  = 0x%08x, ds  = 0x%08x\n",
		ucontext->uc_mcontext.gregs[REG_GS],
		ucontext->uc_mcontext.gregs[REG_FS],
		ucontext->uc_mcontext.gregs[REG_ES],
		ucontext->uc_mcontext.gregs[REG_DS]);
	fprintf_fd(fd,
		"edi = 0x%08x, esi = 0x%08x\nebp = 0x%08x, esp = 0x%08x\n",
		ucontext->uc_mcontext.gregs[REG_EDI],
		ucontext->uc_mcontext.gregs[REG_ESI],
		ucontext->uc_mcontext.gregs[REG_EBP],
		ucontext->uc_mcontext.gregs[REG_ESP]);
	fprintf_fd(fd,
		"eax = 0x%08x, ebx = 0x%08x\necx = 0x%08x, edx = 0x%08x\n",
		ucontext->uc_mcontext.gregs[REG_EAX],
		ucontext->uc_mcontext.gregs[REG_EBX],
		ucontext->uc_mcontext.gregs[REG_ECX],
		ucontext->uc_mcontext.gregs[REG_EDX]);
	fprintf_fd(fd,
		"eip = 0x%08x\n",
		ucontext->uc_mcontext.gregs[REG_EIP]);
};
