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
		"r0   = 0x%08x, r1   = 0x%08x\nr2   = 0x%08x, r3   = 0x%08x\n",
		ucontext->uc_mcontext.arm_r0,
		ucontext->uc_mcontext.arm_r1,
		ucontext->uc_mcontext.arm_r2,
		ucontext->uc_mcontext.arm_r3);
	fprintf_fd(fd,
		"r4   = 0x%08x, r5   = 0x%08x\nr6   = 0x%08x, r7   = 0x%08x\n",
		ucontext->uc_mcontext.arm_r4,
		ucontext->uc_mcontext.arm_r5,
		ucontext->uc_mcontext.arm_r6,
		ucontext->uc_mcontext.arm_r7);
	fprintf_fd(fd,
		"r8   = 0x%08x, r9   = 0x%08x\nr10  = 0x%08x, fp   = 0x%08x\n",
		ucontext->uc_mcontext.arm_r8,
		ucontext->uc_mcontext.arm_r9,
		ucontext->uc_mcontext.arm_r10,
		ucontext->uc_mcontext.arm_fp);
	fprintf_fd(fd,
		"ip   = 0x%08x, sp   = 0x%08x\nlr   = 0x%08x, pc   = 0x%08x\n",
		ucontext->uc_mcontext.arm_ip,
		ucontext->uc_mcontext.arm_sp,
		ucontext->uc_mcontext.arm_lr,
		ucontext->uc_mcontext.arm_pc);
	fprintf_fd(fd, "cpsr = 0x%08x\n",
		ucontext->uc_mcontext.arm_cpsr);
};
