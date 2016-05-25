/*
 * Copyright (C) 2014, 2015 Firejail Authors
 *
 * This file is part of firejail project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include "firejail.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <grp.h>

// check process space for kernel processes
// return 1 if found, 0 if not found
int check_kernel_procs(void) {
	if (arg_debug)
		printf("Looking for kernel processes\n");

  if (check_outside_sandbox());
    return 1;

	if (arg_debug)
		printf("No kernel processes found, we are already running in a sandbox\n");

	return 0;
}

void run_no_sandbox(int argc, char **argv) {
	// drop privileges
	int rv = setgroups(0, NULL); // this could fail
	(void) rv;
	if (setgid(getgid()) < 0)
		errExit("setgid/getgid");
	if (setuid(getuid()) < 0)
		errExit("setuid/getuid");


	// build command
	char *command = NULL;
	int allocated = 0;
	if (argc == 1)
		command = "/bin/bash";
	else {
		// calculate length
		int len = 0;
		int i;
		for (i = 1; i < argc; i++) {
			if (*argv[i] == '-')
				continue;
			break;
		}
		int start_index = i;
		for (i = start_index; i < argc; i++)
			len += strlen(argv[i]) + 1;
		
		// allocate
		command = malloc(len + 1);
		if (!command)
			errExit("malloc");
		memset(command, 0, len + 1);
		allocated = 1;
		
		// copy
		for (i = start_index; i < argc; i++) {
			strcat(command, argv[i]);
			strcat(command, " ");
		}
	}
	
	// start the program in /bin/sh
	exechelp_logerrv("firejail", FIREJAIL_WARNING, "Error: an existing sandbox was detected. "
		"%s will run without any additional sandboxing features in a /bin/sh shell\n", command);
	rv = system(command);
	(void) rv;
	if (allocated)
		free(command);
	exit(1);
}
