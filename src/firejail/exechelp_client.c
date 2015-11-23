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
#include "../include/exechelper-socket.h"
#include "../include/exechelper.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

static char *cmdsocketpath = NULL;

void exechelp_install_socket(void) {
  exechelp_build_run_dir();

  uid_t realuid = getuid();
  gid_t realgid = getgid();

  cmdsocketpath = exechelp_make_socket("commands", realuid, realgid, sandbox_pid);
  if (!cmdsocketpath)
    errExit("exechelp_make_socket");
  if (setenv(EXECHELP_COMMANDS_SOCKET, cmdsocketpath, 1) < 0)
    errExit("setenv");
  if (arg_debug)
    printf("Created a socket at '%s' for client to inform fireexecd of commands it wants to run\n", cmdsocketpath);
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
  if (asprintf(&msg, "register %d %s", sandbox_pid, cmdsocketpath) == -1)
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
