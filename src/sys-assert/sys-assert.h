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


#ifndef _DEBUG_ASSERT_H_
#define _DEBUG_ASSERT_H_

#include <time.h>

#ifdef ARM
#define BASE_LAUNCHPAD_ADDR 0x8000
#else
#define BASE_LAUNCHPAD_ADDR 0x8048000
#endif
#define CALLSTACK_BASE 3

#ifdef __cplusplus
extern "C" {
#endif

	struct addr_node {
		long *startaddr;
		long *endaddr;
		char perm[5];
		char *fpath;
		struct addr_node *next;
	};

#ifdef ARM
	typedef struct layout {
		struct layout *fp;
		void *ret;
	} layout;

#else
	typedef struct layout {
		struct layout *ebp;
		void *ret;
	} layout;
#endif

	extern void *__libc_stack_end;

#ifdef __cplusplus
}
#endif
#endif				/* _DEBUG_ASSERT_H_ */
