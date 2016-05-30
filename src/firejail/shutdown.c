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
	printf ("DEBUG: pid of the sandbox to shutdown: %d\nDEBUG: comm: %s\n", pid, comm);
	if (comm) {
		// remove \n
		char *ptr = strchr(comm, '\n');
		if (ptr)
			*ptr = '\0';
		if (strcmp(comm, "firejail") == 0) {
		  printf ("DEBUG: The process identified by PID is a Firejail sandbox, looking for the child PID\n");
			pid_t child;
			if (find_child(pid, &child) == 0) {
				printf("DEBUG: the first child of pid %d is pid %u\n", pid, child);
				pid = child;
				printf("Switching to pid %u, the first child process inside the sandbox\n", (unsigned) pid);
			} else {
				printf("DEBUG: FATAL ERROR, the child pid could not be found. This explains why Firejail fails to proceed.\n", pid, child);
				printf("DEBUG: As further evidence, here are the permissions associated with the ns folder of the parent pid:\n");
				char *str;
				asprintf(&str, "ls /proc/%d/ -l | grep ns", pid);
				system(str);
			}
		}
		free(comm);
	}

	// check privileges for non-root users
	uid_t uid = getuid();
	if (uid != 0) {
		struct stat s;
		char *dir;
		if (asprintf(&dir, "/proc/%u/ns", pid) == -1)
			errExit("asprintf");
		if (stat(dir, &s) < 0)
			errExit("stat");
		if (s.st_uid != uid) {
			exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: permission is denied to shutdown a sandbox created by a different user (you are %d, the sandbox belongs to %d).\n", uid, s.st_uid);
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
