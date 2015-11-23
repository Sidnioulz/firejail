/* 
    2015 (c) Steve Dodier-Lazaro <sidnioulz@gmail.com>
    This file is part of ExecHelper.

    ExecHelper is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    ExecHelper is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with ExecHelper.  If not, see <http://www.gnu.org/licenses/>.
*/

#define _GNU_SOURCE

#include "../include/common.h"
#include "../include/exechelper.h"
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// build /run/firejail directory
void exechelp_build_run_dir(void) {
	struct stat s;

	if (stat("/run", &s)) {
		if (arg_debug)
			printf("Creating %s directory\n", "/run");
		/* coverity[toctou] */
		int rv = mkdir("/run", S_IRWXU | S_IRWXG | S_IRWXO);
		if (rv == -1)
			errExit("mkdir");
		if (chown("/run", 0, 0) < 0)
			errExit("chown");
		if (chmod("/run", S_IRWXU  | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
			errExit("chmod");
	}
	else { // check /run directory belongs to root end exit if doesn't!
		if (s.st_uid != 0 || s.st_gid != 0) {
			fprintf(stderr, "Error: non-root %s directory, exiting...\n", "/run");
			exit(1);
		}
	}

	if (stat(EXECHELP_RUN_DIR, &s)) {
		if (arg_debug)
			printf("Creating %s directory\n", EXECHELP_RUN_DIR);
		/* coverity[toctou] */
		int rv = mkdir(EXECHELP_RUN_DIR, S_IRWXU | S_IRWXG | S_IRWXO);
		if (rv == -1)
			errExit("mkdir");
		if (chown(EXECHELP_RUN_DIR, 0, 0) < 0)
			errExit("chown");
		if (chmod(EXECHELP_RUN_DIR, S_IRWXU  | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
			errExit("chmod");
	}
	else { // check /run/firejail directory belongs to root end exit if doesn't!
		if (s.st_uid != 0 || s.st_gid != 0) {
			fprintf(stderr, "Error: non-root %s directory, exiting...\n", EXECHELP_RUN_DIR);
			exit(1);
		}
	}
}

#define _EH_HASH_STR_LEN 12
char *exechelp_make_socket(const char *socket_name, const uid_t uid, const gid_t gid, const pid_t pid) {
  char str[_EH_HASH_STR_LEN + 1];
  str[_EH_HASH_STR_LEN] = '\0';
  int available, i;

  // get a file path of the form "/run/firejail/<pid>-commands-<rand>
  char *path = NULL, *dir = NULL;
  int fd;

  if (asprintf(&dir, "/run/firejail/%d/", pid) == -1)
    errExit("asprintf");

  struct stat s;
	if (stat(dir, &s)) {
		if (arg_debug)
			printf("Creating %s directory\n", dir);
		/* coverity[toctou] */
		int rv = mkdir(dir, S_IRWXU | S_IRWXG | S_IRWXO);
		if (rv == -1)
			errExit("mkdir");
		if (chmod(dir, S_IRWXU  | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
			errExit("chmod");
	}
	if (chown(dir, uid, gid) < 0)
		errExit("chown");

  // just in case there are leftovers in there...
  available = 0;
  while (!available) {
    // generate a random string
    for(i = 0; i < 12; i++)
      sprintf(str + i, "%x", rand() % 16);

    // generate a file path
    free(path);
    if (asprintf(&path, "/run/firejail/%d/%s-%s.sock", pid, socket_name, str) == -1)
      errExit("asprintf");

    // check the path is available for a socket (save for race conditions...)
    errno = 0;
    if (access(path, R_OK | W_OK) == -1) {
      if(errno == ENOENT) {
        available = 1;
      } else if (errno == ENOTDIR || errno == EROFS || errno == EFAULT || errno == EINVAL || errno == EINVAL) {
        errExit("access");
      }
    }
  }

  return path;
}

