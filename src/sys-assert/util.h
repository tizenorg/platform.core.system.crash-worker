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

#include "sys-assert.h"

int open_read(const char *path, char *buf, int size);

char *fgets_fd(char *str, int len, int fd);

/* WARNING : formatted string buffer is limited to 1024 byte */
int fprintf_fd(int fd, const char *fmt, ...);

char *remove_path(const char *cmd);

char *get_fpath(long *value, struct addr_node *start);

long *get_start_addr(long *value, struct addr_node *start);
