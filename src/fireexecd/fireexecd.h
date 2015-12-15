/*
 * Copyright (C) 2014, 2015 netblue30 (netblue30@yahoo.com)
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
#ifndef FIREEXECD_H
#define FIREEXECD_H
#define _GNU_SOURCE
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/exechelper.h"
#include "../include/exechelper-logger.h"
#include "../include/pid.h"
#include "../include/common.h"

extern int arg_debug;

typedef enum _client_status_t {
  UNDEFINED=0,
  REGISTERED=1,
  IDENTIFIED=2,
  HANDLED=3,
  ERASED=4,
  ERROR=5
} client_status_t;

// client.c
typedef struct _fireexecd_client_t {
  pid_t                   pid;        // PID of the client
  char                   *cmdpath;    // path of the UNIX socket the client uses to issue commands
  char                   *profile;    // firejail profile of the sandboxed client
  char                   *name;       // name of the client's firejail sandbox
  pid_t                   handler;    // PID of the child that handles communication with the client
  time_t                  regtime;    // timestamp indicating when the client was registered
  client_status_t         status;     // current status of the client
  ExecHelpExecutionPolicy pol;        // policy to apply to the commands received from client
} fireexecd_client_t;

fireexecd_client_t *client_new(pid_t, const char *, const char *, const char *);
void client_delete_socket(fireexecd_client_t *);
void client_free(fireexecd_client_t *);
void client_register(fireexecd_client_t *);
void client_identify_and_handle(fireexecd_client_t *);

void client_list_insert(fireexecd_client_t *);
void client_list_remove(fireexecd_client_t *);
void client_list_clear(void);
fireexecd_client_t *client_list_find_by_pid(pid_t);
fireexecd_client_t *client_list_find_by_command_socket(pid_t);

void client_identities_list_insert(pid_t id);
void client_identities_list_remove(pid_t id);
void client_identities_list_clear(void);
void *client_identities_list_find(pid_t id);

void client_cleanup_for_privileged_daemon(pid_t id);

// client_handler.c
int client_execute_sandboxed(fireexecd_client_t *cli,
                             const char *command,
                             char *argv[], int argc,
                             const char *profile,
                             const char *name,
                             int is_protected_app,
                             int has_protected_files,
                             char **files_to_whitelist);
int client_execute_unsandboxed(fireexecd_client_t *cli,
                             const char *command,
                             char *argv[]);
void client_handle_real(fireexecd_client_t *cli);


// client_dialogs.c
typedef struct _DialogExecData {
  char *caller_command;
  char *command;
  char **argv;
  size_t argc;
  char *profile;
  int sandbox;
  ExecHelpProtectedFileHandler *backup_command;
  ExecHelpProtectedFileHandler *backup_valid_handler;
} DialogExecData;

int client_notify(fireexecd_client_t *cli, const char *icon, const char *header, const char *body);
void client_dialog_run_without_reason(fireexecd_client_t *cli,
                                      const char *caller_command,
                                      const char *command,
                                      char *argv[],
                                      const size_t argc,
                                      char **files_to_whitelist);
void client_dialog_run_incompatible  (fireexecd_client_t *cli,
                                      const char *caller_command,
                                      const char *command,
                                      char *argv[],
                                      const size_t argc,
                                      char **forbidden_files,
                                      char **files_to_whitelist,
                                      ExecHelpProtectedFileHandler *backup_command,
                                      ExecHelpProtectedFileHandler *backup_valid_handler);
// fireexecd.c
extern int arg_nowrap;
int find_child(int id);
void fireexecd_drop_privs(void);
void fireexecd_sleep(int st);

// procevent.c
void procevent(pid_t pid);

// usage.c
void usage(void);

// socket.c
void *registration_run(void *);

#endif
