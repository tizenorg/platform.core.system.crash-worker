/* crash-pipe: handle core file passed from stdin
 *
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
 *
 * Author: Karol Lewandowski <k.lewandowsk@samsung.com>
 */
#define _GNU_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <syslog.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>

#define NELEMS(arr) (sizeof(arr)/sizeof(arr[0]))

enum {
     OPT_HELP,
     OPT_REPORT,
     OPT_SAVE_CORE,
};

const struct option opts[] = {
     { "help", no_argument, 0, OPT_HELP },
     { "report", no_argument, 0, OPT_REPORT },
     { "save-core", required_argument, 0, OPT_SAVE_CORE },
     { 0, 0, 0, 0 }
};

static char *argv0 = "";

static void usage(void)
{
     fprintf(stderr, "usage: %s [--help] [--save-core FILE_NAME] [--report] PID UID GID SIGNAL DUMPTIME EXE\n",
	     argv0);
}

static void report(int argc, char *argv[])
{
     const char *pidstr = argv[0];


     printf("Process crash report: %s\n"
	    "\tpid: %s\n"
	    "\tuid: %s\n"
	    "\tgid: %s\n"
	    "\tsignal: %s\n"
	    "\ttimestamp of crash: %s\n",
	    argv[5], pidstr, argv[1], argv[2], argv[3], argv[4]);

}

static int save_core(const char *core_path)
{
     int fd;
     static char buf[4096];
     int readb, remaining;

     fd = open(core_path, O_WRONLY | O_CREAT | O_EXCL);
     if (fd == -1) {
	  syslog(LOG_ERR, "crash-pipe: Unable to save core file to %s: %s\n",
		 core_path, strerror(errno));
	  return 0;
     }

     while ((readb = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
	  int n;

	  for (n = 0, remaining = readb ; remaining > 0; remaining -= n) {
	       n = write(fd, buf, remaining);
	       if (n == -1) {
		    syslog(LOG_ERR, "crash-pipe: Error while saving core file %s: %s. Removing core.\n",
			   core_path, strerror(errno));
		    (void)unlink(core_path); // XXX check errors here too
		    return 0;
	       }
	  }
     }

     close(fd);

     return 1;
}


int main(int argc, char *argv[])
{
     int i;
     int c;
     int opt_report = 0;
     char *opt_save_core = NULL;
     _Bool ret = 1;

     prctl(PR_SET_DUMPABLE, 0);

     argv0 = argv[0];


     while ((c = getopt_long_only(argc, argv, "", opts, NULL)) != -1) {

	  if (c == OPT_HELP) {
	       usage();
	       exit(EXIT_SUCCESS);
	  }
	  else if (c == OPT_REPORT) {
	       opt_report = 1;
	  }
	  else if (c == OPT_SAVE_CORE) {
	       opt_save_core = strdup(optarg);
	       if (!opt_save_core) {
		    syslog(LOG_CRIT, "Out of memory. Exiting.");
		    exit(EXIT_FAILURE);
	       }
	  }
     }

     argc -= optind;
     argv += optind;

     if (opt_report)
	  report(argc, argv);

     if (opt_save_core)
	  ret = save_core(opt_save_core);

     return ret;
}
