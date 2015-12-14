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
#include "../include/exechelper-datatypes.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static ExecHelpList *clients = NULL;
static ExecHelpList *ids = NULL;

static pthread_mutex_t cli_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t ids_mutex = PTHREAD_MUTEX_INITIALIZER;

fireexecd_client_t *client_new(pid_t pid,
                               const char *cmdpath,
                               const char *profile,
                               const char *name) {
  DBGENTER(pid, "client_new");
  fireexecd_client_t *cli = malloc(sizeof(fireexecd_client_t));
  if(!cli) {
    DBGLEAVE(pid, "client_new");
    return NULL;
  }

  cli->cmdpath = strdup(cmdpath);
  if(!cli->cmdpath) {
    free(cli);
    DBGLEAVE(pid, "client_new");
    return NULL;
  }

  cli->profile = profile ? strdup(profile) : NULL;
  if(profile && !cli->profile) {
    free(cli->cmdpath);
    free(cli);
    DBGLEAVE(pid, "client_new");
    return NULL;
  }

  cli->name = name ? strdup(name) : NULL;
  if(name && !cli->name) {
    free(cli->profile);
    free(cli->cmdpath);
    free(cli);
    DBGLEAVE(pid, "client_new");
    return NULL;
  }

  cli->pid = pid;
  cli->handler = 0;
  cli->regtime = 0;
  cli->status = UNDEFINED;
  cli->pol = EXECHELP_DEFAULT_POLICY;

  DBGLEAVE(pid, "client_new");
  return cli;
}

void client_delete_socket(fireexecd_client_t *cli) {
  DBGENTER(cli?cli->pid:0, "client_delete_socket");
  if (!cli) {
    DBGERR("[n/a]\t\e[01;40;101mERROR:\e[0;0m cannot delete socket for a NULL client\n");
    DBGLEAVE(cli?cli->pid:0, "client_delete_socket");
    return;
  }

  if (cli->status == ERROR) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m client is in an erroneous state, aborting\n", cli->pid);
    DBGLEAVE(cli?cli->pid:0, "client_delete_socket");
    return;
  }

  if (cli->status == ERASED) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m client's socket is already erased, nothing to do\n", cli->pid);
    DBGLEAVE(cli?cli->pid:0, "client_delete_socket");
    return;
  }

  int erasedcmd = cli->cmdpath? 0:1;

  if (cli->cmdpath) {
    int success = 0;
    if (unlink(cli->cmdpath) == -1) {
      if (errno == ENOENT) {
        DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m client's socket '%s' was not found, assuming it has been properly deleted\n", cli->pid, cli->cmdpath);
        success = 1;
      } else
        DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m cannot delete client's socket '%s' (error: %s)\n", cli->pid, cli->cmdpath, strerror(errno));
    } else
      success = 1;

    if (success) {
      free(cli->cmdpath);
      cli->cmdpath = NULL;
      erasedcmd = 1;
    }
  }

  if (erasedcmd) {
    DBGOUT("[%d]\tINFO:  Deleting client's files in /run/firejail...\n", cli->pid);
    // delete the linked/protected apps/files lists
    char *path = NULL;
    if (asprintf(&path, "%s/%d", EXECHELP_RUN_DIR, cli->pid) == -1) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
      DBGLEAVE(cli?cli->pid:-1, "client_delete_socket");
      return;
    }
    if (rmdir(path) == -1)
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m cannot delete client's socket directory '%s' (error: %s)\n", cli->pid, path, strerror(errno));
    free(path);

    if (asprintf(&path, "%s/%d-%s", EXECHELP_RUN_DIR, cli->pid, EXECHELP_LINKED_APPS) == -1) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
      DBGLEAVE(cli?cli->pid:-1, "client_delete_socket");
      return;
    }
    if (unlink(path) == -1)
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m cannot delete client's linked apps file '%s' (error: %s)\n", cli->pid, path, strerror(errno));
    free(path);
    
    if (asprintf(&path, "%s/%d-%s", EXECHELP_RUN_DIR, cli->pid, EXECHELP_PROTECTED_APPS) == -1) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
      DBGLEAVE(cli?cli->pid:-1, "client_delete_socket");
      return;
    }
    if (unlink(path) == -1)
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m cannot delete client's protected apps file '%s' (error: %s)\n", cli->pid, path, strerror(errno));
    free(path);
    
    if (asprintf(&path, "%s/%d-%s", EXECHELP_RUN_DIR, cli->pid, EXECHELP_PROTECTED_FILES) == -1) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
      DBGLEAVE(cli?cli->pid:-1, "client_delete_socket");
      return;
    }
    if (unlink(path) == -1)
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m cannot delete client's protected files file '%s' (error: %s)\n", cli->pid, path, strerror(errno));
    free(path);
    
    if (asprintf(&path, "%s/%d-%s", EXECHELP_RUN_DIR, cli->pid, EXECHELP_WHITELIST_APPS) == -1) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
      DBGLEAVE(cli?cli->pid:-1, "client_delete_socket");
      return;
    }
    if (unlink(path) == -1)
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m cannot delete client's whitelisted apps file '%s' (error: %s)\n", cli->pid, path, strerror(errno));
    free(path);
    
    if (asprintf(&path, "%s/%d-%s", EXECHELP_RUN_DIR, cli->pid, EXECHELP_WHITELIST_FILES) == -1) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
      DBGLEAVE(cli?cli->pid:-1, "client_delete_socket");
      return;
    }
    if (unlink(path) == -1)
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m cannot delete client's whitelisted files file '%s' (error: %s)\n", cli->pid, path, strerror(errno));
    free(path);

    DBGOUT("[%d]\tINFO:  Client successfully cleaned up\n", cli->pid);
    cli->status = ERASED;
  }

  DBGLEAVE(cli?cli->pid:0, "client_delete_socket");
}

void client_free(fireexecd_client_t *cli) {
  pid_t tmp = cli?cli->pid:0;
  DBGENTER(tmp, "client_free");
  if(!cli) {
    DBGLEAVE(tmp, "client_free");
    return;
  }
  
  if (cli->cmdpath) {
    free(cli->cmdpath);
    cli->cmdpath = NULL;
  }
  
  if (cli->profile) {
    free(cli->profile);
    cli->profile = NULL;
  }
  
  if (cli->name) {
    free(cli->name);
    cli->name = NULL;
  }

  free(cli);
  DBGLEAVE(tmp, "client_free");
}

void client_register(fireexecd_client_t *cli) {
  DBGENTER(cli?cli->pid:0, "client_register");
  if(!cli) {
    DBGERR("[n/a]\t\e[01;40;101mERROR:\e[0;0m cannot register a NULL client\n");
    DBGLEAVE(cli?cli->pid:0, "client_register");
    return;
  }

  if (cli->status == ERROR) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m client is in an erroneous state, aborting\n", cli->pid);
    DBGLEAVE(cli?cli->pid:0, "client_register");
    return;
  }

  if (cli->status == IDENTIFIED || cli->status == ERASED || cli->status == HANDLED) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m client has already been registered, nothing to do\n", cli->pid);
    DBGLEAVE(cli?cli->pid:0, "client_register");
    return;
  }

  cli->regtime = time(NULL);
  cli->status = REGISTERED;
  DBGLEAVE(cli?cli->pid:0, "client_register");
}

#define _CLIENT_AUTH_COUNTER_LIMIT_SECS   10
#define _CLIENT_AUTH_COUNTER_LIMIT_200MS  50
static void *client_identify_and_handle_real(void *user_data) {
  fireexecd_client_t *cli = user_data;
  DBGENTER(cli?cli->pid:0, "client_identify_and_handle_real");
  struct timespec rqtp = {0, 200000000};
  int counter = 0;

  DBGOUT("[%d]\tINFO:  Starting to identify client...\n", cli->pid);
  while(cli->status == REGISTERED && counter < _CLIENT_AUTH_COUNTER_LIMIT_200MS) {
    if(client_identities_list_find(cli->pid)) {
      cli->status = IDENTIFIED;
    } else {
      nanosleep(&rqtp, NULL);
      counter++;
    }
  }

  // 10 seconds elapsed without authenticating client...
  if(counter == _CLIENT_AUTH_COUNTER_LIMIT_200MS) {
    //TODO log: there was an unauthorised attempt to register
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m client attempted to register but could not be confirmed to be a firejail launcher in the past %d seconds, aborting\n", cli->pid, _CLIENT_AUTH_COUNTER_LIMIT_SECS);
    client_list_remove(cli);
    cli->status = ERROR;
    return NULL;
  }

  DBGOUT("[%d]\tINFO:  Client successfully identified as a firejail process, starting to handle it\n", cli->pid);
  pid_t spoon = fork();
  if (spoon == -1) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m could not fork the daemon to handle client (error: %s)\n", cli->pid, strerror(errno));
    client_list_remove(cli);
    cli->status = ERROR;
    DBGLEAVE(cli?cli->pid:0, "client_identify_and_handle_real");
    return NULL;
  }

  // parent process has no more work to do, will only need to erase socket when the client is disconnected
  cli->status = HANDLED;

  if (spoon == 0) {
    client_handle_real(cli);
    exit(-1); // exiting in case client_handle_real forgets to
  } else {
    cli->handler = spoon;
  }

  DBGLEAVE(cli?cli->pid:0, "client_identify_and_handle_real");
  return NULL;
}

void client_identify_and_handle(fireexecd_client_t *cli) {
  DBGENTER(cli?cli->pid:0, "client_identify_and_handle");
  if(!cli) {
    DBGERR("[n/a]\t\e[01;40;101mERROR:\e[0;0m cannot identify and handle a NULL client\n");
    DBGLEAVE(cli?cli->pid:0, "client_identify_and_handle");
    return;
  }

  pthread_t thread;
  if (pthread_create(&thread, NULL, client_identify_and_handle_real, cli) == -1) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m could not create a thread to identify and handle client (error: %s)\n", cli->pid, strerror(errno));
    cli->status = ERROR;
    DBGLEAVE(cli?cli->pid:0, "client_identify_and_handle");
    return;
  }

  // we ignore errors, we're confident the id exists and if the thread's not joinable it means it's done running
  pthread_detach(thread);
  DBGLEAVE(cli?cli->pid:0, "client_identify_and_handle");
}

void client_list_insert(fireexecd_client_t *cli) {
  DBGENTER(cli?cli->pid:0, "client_list_insert");
  if(!cli) {
    DBGERR("[n/a]\t\e[01;40;101mERROR:\e[0;0m cannot insert a NULL client\n");
    DBGLEAVE(cli?cli->pid:0, "client_list_insert");
    return;
  }

  if (cli->status == ERROR) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m client is in an erroneous state, aborting\n", cli->pid);
    DBGLEAVE(cli?cli->pid:0, "client_list_insert");
    return;
  }

  if (cli->status != REGISTERED) {
    DBGERR("[%d]\t\e[02;30;103mWARN:\e[0;0m  client must be registered and not yet connected when inserted into the current client list\n", cli->pid);
    DBGLEAVE(cli?cli->pid:0, "client_list_insert");
    return;
  }

  if (client_list_find_by_pid(cli->pid)) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m client has already been inserted in the current client list\n", cli->pid);
    DBGLEAVE(cli?cli->pid:0, "client_list_insert");
    return;
  }

  pthread_mutex_lock(&cli_mutex);
  clients = exechelp_list_prepend(clients, cli);
  pthread_mutex_unlock(&cli_mutex);
  DBGLEAVE(cli?cli->pid:0, "client_list_insert");
}

void client_list_remove(fireexecd_client_t *cli) {
  DBGENTER(cli?cli->pid:0, "client_list_remove");
  pthread_mutex_lock(&cli_mutex);
  clients = exechelp_list_remove(clients, cli);
  pthread_mutex_unlock(&cli_mutex);
  DBGLEAVE(cli?cli->pid:0, "client_list_remove");
}

void client_list_clear(void) {
  DBGENTER(0, "client_list_clear");
  pthread_mutex_lock(&cli_mutex);
  exechelp_list_free(clients);
  clients = NULL;
  pthread_mutex_unlock(&cli_mutex);
  DBGLEAVE(0, "client_list_clear");
}

void client_identities_list_insert(pid_t id) {
  DBGENTER(id, "client_identities_list_insert");
  if(!id) {
    DBGERR("[n/a]\t\e[01;40;101mERROR:\e[0;0m cannot insert a NULL client identity\n");
    DBGLEAVE(id, "client_identities_list_insert");
    return;
  }

  if (client_identities_list_find(id)) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m PID has already been inserted in the current client identity list\n", id);
    DBGLEAVE(id, "client_identities_list_insert");
    return;
  }

  pthread_mutex_lock(&ids_mutex);
  ids = exechelp_list_prepend(ids, EH_INT_TO_POINTER(id));
  pthread_mutex_unlock(&ids_mutex);
  DBGLEAVE(id, "client_identities_list_insert");
}

void client_identities_list_remove(pid_t id) {
  DBGENTER(id, "client_identities_list_remove");
  pthread_mutex_lock(&ids_mutex);
  ids = exechelp_list_remove(ids, EH_INT_TO_POINTER(id));
  pthread_mutex_unlock(&ids_mutex);
  DBGLEAVE(id, "client_identities_list_remove");
}

void client_identities_list_clear(void) {
  DBGENTER(0, "client_identities_list_clear");
  pthread_mutex_lock(&ids_mutex);
  exechelp_list_free(ids);
  ids = NULL;
  pthread_mutex_unlock(&ids_mutex);
  DBGLEAVE(0, "client_identities_list_clear");
}

static int client_pid_equal (const void *list_item, const void *user_data) {
  const fireexecd_client_t *cli = list_item;
  const pid_t pid = EH_POINTER_TO_INT(user_data);
  return (cli->pid - pid);
}

fireexecd_client_t *client_list_find_by_pid(pid_t pid) {
  DBGENTER(pid, "client_list_find_by_pid");
  pthread_mutex_lock(&cli_mutex);
  ExecHelpList *ptr = exechelp_list_find_custom(clients, EH_INT_TO_POINTER(pid), client_pid_equal);
  pthread_mutex_unlock(&cli_mutex);
  DBGLEAVE(pid, "client_list_find_by_pid");
  return ptr? ptr->data:NULL;
}

fireexecd_client_t *client_list_find_by_command_socket(pid_t);

void *client_identities_list_find(pid_t id) {
  DBGENTER(id, "client_identities_list_find");
  pthread_mutex_lock(&ids_mutex);
  ExecHelpList *ptr = exechelp_list_find(ids, EH_INT_TO_POINTER(id));
  pthread_mutex_unlock(&ids_mutex);
  DBGLEAVE(id, "client_identities_list_find");
  return ptr? ptr->data:NULL;
}

void client_cleanup_for_privileged_daemon(pid_t id) {
  DBGENTER(id, "client_cleanup_for_privileged_daemon");
  fireexecd_client_t *cli = client_list_find_by_pid(id);
  if (!cli) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m cannot find a client for PID %d, no longer trying to cleanup\n", id, id);
    DBGLEAVE(id, "client_cleanup_for_privileged_daemon");
    return;
  }

  if (cli->status != HANDLED) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m client is not currently handled, nothing to cleanup\n", id);
    DBGLEAVE(id, "client_cleanup_for_privileged_daemon");
    return;
  }

  struct timespec rqtp = {0, 200000000};
  int count = 0;
  int joined = 0;

  // wait for child to finish for up to one second
  while(!joined && count++ < 5) {
    int stat_loc;
    int ret = waitpid(cli->handler, &stat_loc, WNOHANG);
    if (ret == -1) {
      if (errno == ECHILD)
        joined = 1;
      else
        DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m error received when waiting on fireexecd handler (error: %s)\n", id, strerror(errno));
    }
    else 
      joined = (WIFEXITED(stat_loc));

    if(!joined)
      nanosleep(&rqtp, NULL);
  }

  if(!joined) {
    DBGOUT("[%d]\tINFO:  fireexecd handler for client is still alive, killing it\n", id);
    if (kill(cli->handler, SIGKILL) == -1)
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to kill fireexecd handler (error: %s)\n", id, strerror(errno));
  }

  client_list_remove(cli);
  client_identities_list_remove(id);
  client_delete_socket(cli);
  client_free(cli);
  DBGLEAVE(id, "client_cleanup_for_privileged_daemon");
}
