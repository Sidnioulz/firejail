/*
 * Copyright (C) 2014, 2015 Firejail Authors
 * Copyright (C) 2015 Steve Dodier-Lazaro (sidnioulz@gmail.com)
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
#include "../include/exechelper.h"
#include "../include/exechelper-logger.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


static char *cmdsocketpath = NULL;

void exechelp_propagate_sandbox_info_to_env(void) {
  // check if the command to be run was originally a protected app
  int protected = is_current_command_protected();
  if (arg_debug)
    printf("Child process is %sa protected application\n", protected?"":"not ");

  // if we're not protected yet, process the argument list
  char *protected_path = NULL;
  if (!protected) {
    char *protected_files = exechelp_read_list_from_file(PROTECTED_FILES_SB_PATH);
    if (!protected_files)
      errExit("exechelp_read_list_from_file protected apps");
  
		for (int i = cfg.original_program_index; i < cfg.original_argc && !protected; i++) {
			if (cfg.original_argv[i] == NULL)
				break;

      if (exechelp_file_list_contains_path(protected_files, cfg.original_argv[i], NULL))
        protected = 1;
    }

    if(arg_debug)
      printf("Child process %s protected files\n", protected?"targets":"does not target");
  }

  // set environment variables for libexechelper to intercept...
  if (protected) {
    exechelp_logv("firejail", "Declaring sandbox as protected\n");
    if (setenv(EXECHELP_SANDBOX_TYPE_ENV, "protected", 1) < 0)
      errExit("setenv");
  } else {
    exechelp_logv("firejail", "Declaring sandbox as untrusted\n");
    if (setenv(EXECHELP_SANDBOX_TYPE_ENV, "untrusted", 1) < 0)
      errExit("setenv");
  }

  if (cfg.hostname) {
    if (setenv(EXECHELP_SANDBOX_NAME_ENV, cfg.hostname, 1) < 0)
      errExit("setenv");
  }

  if (arg_debug) {
    char *level;
    if (asprintf(&level, "%d", arg_debug) == -1)
      errExit("asprintf");
    if (setenv(EXECHELP_DEBUG_ENV, level, 1) < 0)
      errExit("setenv");
    free(level);
  }
}

#define _EH_HASH_STR_LEN 12
static char *exechelp_gen_socket_name(const char *socket_name, const uid_t uid, const gid_t gid, const pid_t pid) {
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

void exechelp_install_socket(void) {
  exechelp_build_run_dir();

  uid_t realuid = getuid();
  gid_t realgid = getgid();

  cmdsocketpath = exechelp_gen_socket_name("commands", realuid, realgid, sandbox_pid);
  if (!cmdsocketpath)
    errExit("exechelp_gen_socket_name");
  exechelp_set_socket_env_manually(cmdsocketpath);
  if (arg_debug)
    printf("Created a socket at '%s' for client to inform fireexecd of commands it wants to run\n", cmdsocketpath);
}

void exechelp_set_socket_env_from_pid(pid_t pid) {
  char *socketdir, *socketpath = NULL;
  if (asprintf(&socketdir, EXECHELP_RUN_DIR"/%d", pid-1) == -1)
    errExit("asprintf");

  DIR* dir = opendir(socketdir);
  if(dir == NULL)
    errExit("opendir");

  struct dirent *result = NULL;
  struct dirent *entry = malloc(sizeof(struct dirent));
  int found = 0;
  if(!entry)
    errExit("malloc");

  while(!found) {
    if (readdir_r(dir, entry, &result)) {
      fprintf(stderr, "Error when reading runtime directory %s to find child process's socket: %s\n", socketdir, strerror(errno));
      continue;
    }

    if(!result)
      break;

    if((strcmp(entry->d_name, ".") != 0) && (strcmp(entry->d_name, "..") != 0)) {
      if (asprintf(&socketpath, "%s/%s", socketpath, entry->d_name) == -1)
        errExit("asprintf");
      found = 1;
    }
  }

  if(closedir(dir))
    errExit("closedir");
  free(socketdir);

  if (socketpath) {
    exechelp_set_socket_env_manually(socketpath);
    free(socketpath);
  } else {
    fprintf(stderr, "Could not find any socket in child process's runtime directory, aborting\n");
    exit(-1);
  }
}

void exechelp_set_socket_env_manually(char *cmdsocketpath) {
  if(!cmdsocketpath)
    return;

  if (setenv(EXECHELP_COMMANDS_SOCKET, cmdsocketpath, 1) < 0)
    errExit("setenv");
}

void exechelp_register_socket(void) {
  if (!cmdsocketpath) {
    fprintf(stderr, "Error: exechelp_register_socket called before its socket was initialised, check the firejail source code.\n");
    exit (-1);
  }

  // connect to fireexecd
  int s;
  if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    errExit("socket");

  struct sockaddr_un remote;
  remote.sun_family = AF_UNIX;
  strcpy(remote.sun_path, EXECHELP_REGISTRATION_SOCKET);
  socklen_t len = strlen(remote.sun_path) + sizeof(remote.sun_family);
  if (connect(s, (struct sockaddr *)&remote, len) == -1) {
    fprintf(stderr, "Error: the --helper option was given but no execution helper daemon is running. Please run 'firexecd' prior to running client processes with the --helper option.\n");
    errExit("connect"); 
  }

  // message fireexecd to register the socket paths
  char *msg;
  if (asprintf(&msg, "register %d %s %s %s", sandbox_pid, cmdsocketpath, cfg.profile_name, cfg.hostname) == -1)
    errExit("asprintf");

  if (send(s, msg, strlen(msg), 0) == -1)
    errExit("send");
  free(msg);

  // verify fireexecd agreed
  char reply[EXECHELP_REGISTRATION_MSGLEN + 1];
  reply[EXECHELP_REGISTRATION_MSGLEN] = '\0';
  ssize_t received;
  if ((received = recv(s, reply, EXECHELP_REGISTRATION_MSGLEN, 0)) > 0) {
      reply[received] = '\0';
      char *pol = NULL;
      if(strcmp("ACK", reply) != 0) {
        fprintf(stderr, "Warning: the execution helper daemon did not allow us to proceed with sending commands for remote execution, setting a client policy that allows all executions internally.\n");
        if (asprintf(&pol, "%d", SANDBOX_ITSELF) == -1)
          errExit("asprintf");
      } else {
        if (asprintf(&pol, "%d", EXECHELP_DEFAULT_POLICY) == -1)
          errExit("asprintf");
      }
      if (setenv(EXECHELP_EXECUTION_POLICY, pol, 1) == -1)
        errExit("setenv");

      free(pol);
  } else {
      if (received < 0)
        errExit("recv");
      else {
        fprintf(stderr, "Error: the execution helper daemon has closed the connection. Please verify 'firexecd' functions properly prior to running client processes with the --helper option.\n");
        exit(1);
      }
  }

  close(s);
}
