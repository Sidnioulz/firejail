/*
 * Copyright (C) 2015 Steve Dodier-Lazaro (sidnioulz@gmail.com)
 *
 * This file is part of firejail project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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
#include "fireexecd.h"
#include "../include/exechelper.h"
#include <limits.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

int regsocket;

static void init_registration_socket(void) {
  DBGENTER(0, "init_registration_socket");
  if ((regsocket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    errExit("socket");

  exechelp_build_run_dir();

  struct sockaddr_un local;
  local.sun_family = AF_UNIX;
  strcpy(local.sun_path, EXECHELP_REGISTRATION_SOCKET);
  unlink(local.sun_path);
  size_t len = strlen(local.sun_path) + sizeof(local.sun_family);
  if (bind(regsocket, (struct sockaddr *)&local, len) == -1)
    errExit("bind");

  if (listen(regsocket, SOMAXCONN) == -1)
    errExit("listen");
  DBGLEAVE(0, "init_registration_socket");
}

static fireexecd_client_t *process_registration(int clisocket, char *ptr) {
  DBGENTER(0, "process_registration");
  char *endptr = NULL;

  // get the PID
  errno = 0;
  long pid = strtol(ptr, &endptr, 10);

  if ((errno == ERANGE && (pid == LONG_MAX || pid == LONG_MIN)) || (errno != 0 && pid == 0)) {
    DBGERR("[n/a]\t\e[01;40;101mERROR:\e[0;0m client socket %d sent a broken registration packet, aborting connection\n", clisocket);
    DBGLEAVE(0, "process_registration");
    return NULL;
  }

  if (endptr == ptr) {
    DBGERR("[n/a]\t\e[01;40;101mERROR:\e[0;0m client socket %d sent an incomplete registration packet (missing PID), aborting connection\n", clisocket);
    DBGLEAVE(0, "process_registration");
    return NULL;
  }

  if (*endptr == '\0') {
    DBGERR("[n/a]\t\e[01;40;101mERROR:\e[0;0m client socket %d sent an incomplete registration packet (missing socket path), aborting connection\n", clisocket);
    DBGLEAVE(0, "process_registration");
    return NULL;
  }

  // position endptr to the space between both paths, and cut them
  char *cmdpath = endptr+1;

  // pointer to next bit after cmdpath
  char *next = strchr(cmdpath, ' ');
  char *profile = NULL;
  char *name = NULL;

  if (next) {
    next[0] = '\0';
    next++;

    char *nnext = strchr(next, ' ');
    if (nnext) {
      nnext[0] = '\0';
      nnext++;
      if (strcmp(nnext, "(null)")) {
        name = nnext;
      }
    }

    if (strcmp(next, "(null)")) {
      profile = next;
    }
  }

  DBGOUT("[n/a]\tINFO:  new client registered: PID %d, Command socket '%s', Profile '%s', Name '%s'\n", (pid_t) pid, cmdpath, profile, name);
  fireexecd_client_t *client = client_new(pid, cmdpath, profile, name);
  if(!client)
    errExit("client_new");

  client_register(client);
  client_list_insert(client);

  DBGLEAVE(0, "process_registration");
  return client;
}

static void accept_registrations(void) {
  DBGENTER(0, "accept_registrations");
  int clisocket;
  struct sockaddr_un remote;
  socklen_t t;

  char msg[EXECHELP_REGISTRATION_MSGLEN + 1];
  msg[EXECHELP_REGISTRATION_MSGLEN] = '\0';

  while(1) {
    t = sizeof(remote);
    if ((clisocket = accept(regsocket, (struct sockaddr *)&remote, &t)) == -1) {
      DBGERR("[n/a]\t\e[01;40;101mERROR:\e[0;0m failed to accept a new connection (error: %s)\n", strerror(errno));
      continue;
    }

    ssize_t n = recv(clisocket, msg, EXECHELP_REGISTRATION_MSGLEN, 0);
    if (n <= 0) {
      DBGERR("[n/a]\t\e[01;40;101mERROR:\e[0;0m client socket %d failed to send a registration packet, aborting connection\n", clisocket);
      close(clisocket);
      continue;
    }
    msg[n] = '\0';
    
    char *ptr = msg;
    char *cmdend = strchr(ptr, ' ');
    if (!cmdend) {
      DBGERR("[n/a]\t\e[01;40;101mERROR:\e[0;0m client socket %d sent a broken registration packet, aborting connection\n", clisocket);
      close(clisocket);
      continue;
    }
    
    cmdend[0] = '\0';
    if (strcmp(ptr, "register") == 0) {
      ptr = cmdend+1;

      fireexecd_client_t *client = process_registration(clisocket, ptr);
      if (client) {
        DBGOUT("[%d]\tINFO:  client has been successfully registered\n", client->pid);
        client->pol = EXECHELP_DEFAULT_POLICY;
      }

      char *msg = client? "ACK" : "DENIED";
      if (send(clisocket, msg, strlen(msg), 0) < 0) {
        DBGERR("[n/a]\t\e[01;40;101mERROR:\e[0;0m failed to send a reply to client socket %d, aborting connection\n", clisocket);
      }

      if(client)
        client_identify_and_handle(client);
    } else {
      DBGERR("[n/a]\t\e[01;40;101mERROR:\e[0;0m client %d sent an unknown command (%s) in its registration packet, aborting connection\n", clisocket, ptr);
    }
    
    close(clisocket);
  }
  DBGLEAVE(0, "accept_registrations");
}

void *registration_run(void *ignored) {
  DBGENTER(0, "registration_run");
  init_registration_socket();
  accept_registrations();
  DBGLEAVE(0, "registration_run");
  return NULL;
}

