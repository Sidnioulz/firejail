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
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/prctl.h>

void shut_name(const char *name) {
	if (!name || strlen(name) == 0) {
		exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: invalid sandbox name\n");
		exit(1);
	}
	
	pid_t pid;
	if (name2pid(name, &pid)) {
		exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: cannot find sandbox %s\n", name);
		exit(1);
	}

	shut(pid);
}

void shut(pid_t pid) {
	pid_t parent = pid;
	// if the pid is that of a firejail  process, use the pid of a child process inside the sandbox
	char *comm = pid_proc_comm(pid);
	if (comm) {
		// remove \n
		char *ptr = strchr(comm, '\n');
		if (ptr)
			*ptr = '\0';
		if (strcmp(comm, "firejail") == 0) {
			pid_t child;
			if (find_child(pid, &child) == 0) {
				pid = child;
				printf("Switching to pid %u, the first child process inside the sandbox\n", (unsigned) pid);
			}
		}
		free(comm);
	}

	// check privileges for non-root users
	uid_t uid = getuid();
	if (uid != 0) {
		uid_t sandbox_uid = pid_get_uid(pid);
		if (uid != sandbox_uid) {
			exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: permission is denied to shutdown a sandbox created by a different user.\n");
			exit(1);
		}
	}

	printf("Sending SIGTERM to %u\n", pid);
	kill(pid, SIGTERM);
	sleep(2);
	
	// if the process is still running, terminate it using SIGKILL
	// try to open stat file
	char *file;
	if (asprintf(&file, "/proc/%u/status", pid) == -1) {
		exechelp_perror("firejail", "asprintf");
		exit(1);
	}
	FILE *fp = fopen(file, "r");
	if (!fp)
		return;
	fclose(fp);
	
	// kill the process and also the parent
	printf("Sending SIGKILL to %u\n", pid);
	kill(pid, SIGKILL);
	if (parent != pid) {
		printf("Sending SIGKILL to %u\n", parent);
		kill(parent, SIGKILL);
	}
}
