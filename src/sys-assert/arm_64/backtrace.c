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
#include <libunwind.h>
#include <execinfo.h>
#include "util.h"

int dump_callstack(void **callstack_addrs, int size, void *context, int retry)
{
	ucontext_t *ucontext = context;
	int count;

	if (!callstack_addrs)
		return 0;

	if (!retry && (context && ((int)ucontext->uc_mcontext.pc != 0)))
		count = unw_backtrace(callstack_addrs, size);
	else
		count = backtrace(callstack_addrs, size);

	if (count > CALLSTACK_BASE) {
		count -= CALLSTACK_BASE;
	} else if (context) {
		callstack_addrs[CALLSTACK_BASE] = (long *)ucontext->uc_mcontext.pc;
		callstack_addrs[CALLSTACK_BASE] = (long *)ucontext->uc_mcontext.regs[30]; /* LR (link register) */
		count = 2;
	} else {
		count = 0;
	}
	return count;
};
