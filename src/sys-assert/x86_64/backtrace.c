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
#include <execinfo.h>
#include <ucontext.h>
#include "sys-assert.h"
#include "util.h"
#include "sys-assert-regs.h"

int dump_callstack(void **callstack_addrs, int size, void *context, int retry)
{
	ucontext_t *ucontext = context;
	int count = CALLSTACK_BASE;

	if (!callstack_addrs)
		return 0;

	if (context) {
		layout *ebp = (layout *)ucontext->uc_mcontext.gregs[REG_EBP];
		callstack_addrs[count++] =
			(long *)ucontext->uc_mcontext.gregs[REG_EIP];
		while (ebp && (count < size)) {
			callstack_addrs[count++] = ebp->ret;
			ebp = ebp->ebp;
		}
	} else {
		count = backtrace(callstack_addrs, size);
	}

	if (count > CALLSTACK_BASE) {
		count -= CALLSTACK_BASE;
	} else if (context) {
		callstack_addrs[CALLSTACK_BASE] = (long *)ucontext->uc_mcontext.gregs[REG_EIP];
		callstack_addrs[CALLSTACK_BASE + 1] = (long *)ucontext->uc_mcontext.gregs[REG_ESP];
		count = 2;
	} else {
		count = 0;
	}
	return count;
};
