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
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include "util.h"

int open_read(const char *path, char *buf, int size)
{
	int fd;
	int ret;

	if (buf == NULL || path == NULL)
	    return -1;

	fd = open(path, O_RDONLY);
	if (fd < 0)
	    return -1;

	ret = read(fd, buf, size - 1);
	if (ret <= 0) {
	    close(fd);
	    return -1;
	} else
	    buf[ret] = '\0';

	close(fd);

	return ret;
}

char *fgets_fd(char *str, int len, int fd)
{
	char ch;
	register char *cs;
	int num = 0;

	cs = str;
	while (--len > 0 && (num = read(fd, &ch, 1) > 0)) {
		if ((*cs++ = ch) == '\n')
			break;
	}
	*cs = '\0';
	return (num == 0 && cs == str) ? NULL : str;
}

/* WARNING : formatted string buffer is limited to 1024 byte */
int fprintf_fd(int fd, const char *fmt, ...)
{
	int n, ret;
	char buff[1024];
	va_list args;

	va_start(args, fmt);
	n = vsnprintf(buff, 1024 - 1, fmt, args);
	ret = write(fd, buff, n);
	if (ret < 0)
		fprintf(stderr, "write failed\n");
	va_end(args);
	return n;
}

char *remove_path(const char *cmd)
{
	char *cp;
	char *np;

	cp = np = (char *)cmd;
	while (*cp && *cp != ' ') {
		if (*cp == '/')
			np = cp + 1;
		cp++;
	}
	return np;
}

char *get_fpath(long *value, struct addr_node *start)
{
	struct addr_node *t_node;
	struct addr_node *n_node;

	if (value == 0 || start == NULL)
		return NULL;
	t_node = start;
	n_node = t_node->next;
	while (n_node) {
		if (t_node->endaddr <= value) {
			t_node = n_node;
			n_node = n_node->next;
		} else if (t_node->startaddr <= value) {
			return t_node->fpath;
		} else
			break;
	}
	return NULL;
}

long *get_start_addr(long *value, struct addr_node *start)
{
	struct addr_node *t_node;
	struct addr_node *n_node;

	if (value == 0 || start == NULL)
		return NULL;
	t_node = start;
	n_node = t_node->next;
	while (n_node) {
		if (t_node->endaddr <= value) {
			t_node = n_node;
			n_node = n_node->next;
		} else if (t_node->startaddr <= value) {
			return t_node->startaddr;
		} else
			break;
	}
	return NULL;
}
