/*
 * SYS-ASSERT
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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

#ifndef __SYS_ASSERT_REGS_H__
#define __SYS_ASSERT_REGS_H__

#include <ucontext.h>

#ifndef REG_GS
#define REG_GS 0
#endif

#ifndef REG_FS
#define REG_FS 1
#endif

#ifndef REG_ES
#define REG_ES 2
#endif

#ifndef REG_DS
#define REG_DS 3
#endif

#ifndef REG_EDI
#define REG_EDI 4
#endif

#ifndef REG_ESI
#define REG_ESI 5
#endif

#ifndef REG_EBP
#define REG_EBP 6
#endif

#ifndef REG_ESP
#define REG_ESP 7
#endif

#ifndef REG_EBX
#define REG_EBX 8
#endif

#ifndef REG_EDX
#define REG_EDX 9
#endif

#ifndef REG_ECX
#define REG_ECX 10
#endif

#ifndef REG_EAX
#define REG_EAX 11
#endif

#ifndef REG_TRAPNO
#define REG_TRAPNO 12
#endif

#ifndef REG_ERR
#define REG_ERR 13
#endif

#ifndef REG_EIP
#define REG_EIP 14
#endif

#ifndef REG_CS
#define REG_CS 15
#endif

#ifndef REG_EFL
#define REG_EFL 16
#endif

#ifndef REG_UESP
#define REG_UESP 17
#endif

#ifndef REG_SS
#define REG_SS 18
#endif

#endif /* __SYS_ASSERT_REGS_H__ */
