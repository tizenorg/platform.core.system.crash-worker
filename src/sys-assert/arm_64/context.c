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
		"x0   = 0x%08x, x1   = 0x%08x\nx2   = 0x%08x, x3   = 0x%08x\n",
		ucontext->uc_mcontext.regs[0],
		ucontext->uc_mcontext.regs[1],
		ucontext->uc_mcontext.regs[2],
		ucontext->uc_mcontext.regs[3]);
	fprintf_fd(fd,
		"x4   = 0x%08x, x5   = 0x%08x\nx6   = 0x%08x, x7   = 0x%08x\n",
		ucontext->uc_mcontext.regs[4],
		ucontext->uc_mcontext.regs[5],
		ucontext->uc_mcontext.regs[6],
		ucontext->uc_mcontext.regs[7]);
	fprintf_fd(fd,
		"x8   = 0x%08x, x9   = 0x%08x\nx10  = 0x%08x, x11  = 0x%08x\n",
		ucontext->uc_mcontext.regs[8],
		ucontext->uc_mcontext.regs[9],
		ucontext->uc_mcontext.regs[10],
		ucontext->uc_mcontext.regs[11]);
	fprintf_fd(fd,
		"x12  = 0x%08x, x13  = 0x%08x\nx14  = 0x%08x, x15  = 0x%08x\n",
		ucontext->uc_mcontext.regs[12],
		ucontext->uc_mcontext.regs[13],
		ucontext->uc_mcontext.regs[14],
		ucontext->uc_mcontext.regs[15]);
	fprintf_fd(fd,
		"x16  = 0x%08x, x17  = 0x%08x\nx18  = 0x%08x, x19  = 0x%08x\n",
		ucontext->uc_mcontext.regs[16],
		ucontext->uc_mcontext.regs[17],
		ucontext->uc_mcontext.regs[18],
		ucontext->uc_mcontext.regs[19]);
	fprintf_fd(fd,
		"x20  = 0x%08x, x21  = 0x%08x\nx22  = 0x%08x, x23  = 0x%08x\n",
		ucontext->uc_mcontext.regs[20],
		ucontext->uc_mcontext.regs[21],
		ucontext->uc_mcontext.regs[22],
		ucontext->uc_mcontext.regs[23]);
	fprintf_fd(fd,
		"x24  = 0x%08x, x25  = 0x%08x\nx26  = 0x%08x, x27  = 0x%08x\n",
		ucontext->uc_mcontext.regs[24],
		ucontext->uc_mcontext.regs[25],
		ucontext->uc_mcontext.regs[26],
		ucontext->uc_mcontext.regs[27]);
	fprintf_fd(fd,
		"x28  = 0x%08x, x29  = 0x%08x\nx30  = 0x%08x\n",
		ucontext->uc_mcontext.regs[28],
		ucontext->uc_mcontext.regs[29],
		ucontext->uc_mcontext.regs[30]);
	fprintf_fd(fd,
		"xr   = 0x%08x, ip0  = 0x%08x\nip1  = 0x%08x, pr   = 0x%08x\n",
		ucontext->uc_mcontext.regs[8],   /* Indirect result location register */
		ucontext->uc_mcontext.regs[16],  /* Intra-procedure call scratch register 0 */
		ucontext->uc_mcontext.regs[17],  /* Intra-procedure call scratch register 1 */
		ucontext->uc_mcontext.regs[18]); /* Platform register */
	fprintf_fd(fd,
		"fp   = 0x%08x, lr   = 0x%08x\npc   = 0x%08x, sp   = 0x%08x\n",
		ucontext->uc_mcontext.regs[29],  /* Frame register */
		ucontext->uc_mcontext.regs[30],  /* Procedure link register */
		ucontext->uc_mcontext.pc,        /* Program counter */
		ucontext->uc_mcontext.sp);       /* Stack pointer */
};
