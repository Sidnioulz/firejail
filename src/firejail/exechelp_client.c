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
#include <proc/readproc.h>


static char *cmdsocketpath = NULL;

static FILE *get_domain_env_w(const pid_t pid, int reset) {
  static FILE *fp = NULL;

  if (fp == NULL && !reset) {
    char *env_path;
    if (asprintf(&env_path, "%s/%d/%s", EXECHELP_RUN_DIR, pid, DOMAIN_ENV_FILE) == -1)
      errExit("asprintf");

    fp = fopen(env_path, "wb");
    free(env_path);
  } else if (fp && reset) {
    fclose(fp);
    fp = NULL;
  }

  return fp;
}

void load_domain_env(const pid_t pid) {
  char *env_path;
  if (asprintf(&env_path, "%s/%d/%s", EXECHELP_RUN_DIR, pid, DOMAIN_ENV_FILE) == -1)
    errExit("asprintf");
  FILE *fp = fopen(env_path, "rb");
  if (!fp) {
    exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error when loading environment variables list for Firejail domain '%d': %s\n", pid, strerror(errno));
    exit(-1);
  }

  free(env_path);

  char *buffer = NULL;
  char *value;
  size_t n = 0;
  ssize_t linelen = 0;
  errno = 0;
  while ((linelen = getline(&buffer, &n, fp)) != -1) {
    if (buffer[linelen-1] == '\n')
      buffer[linelen-1] = '\0';

    value = strchr(buffer, '=');
    if (value) {
      *value = 0;
      value += 1;

      if (arg_debug)
        printf ("Setting environment variable %s to value '%s'\n", buffer, value);
      setenv(buffer, value, 1);
    } else {
      exechelp_logerrv("firejail", FIREJAIL_WARNING, "Warning: the following line from firejail domain's %d list of environment variables was not recognised: %s\n", pid, buffer);
    }
  
    free(buffer);
    buffer = NULL;
    n = 0;
  }

  fclose(fp);
}

static void load_relevant_env_variable(const char *name, const char *value) {
  if (!name || !value)
    return;

  if (strcmp(name, EXECHELP_SANDBOX_TYPE_ENV) == 0 ||
      strcmp(name, EXECHELP_SANDBOX_NAME_ENV) == 0 ||
      strcmp(name, EXECHELP_SANDBOX_LOCK_WS_ENV) == 0 ||
      strcmp(name, EXECHELP_DEBUG_ENV) == 0 ||
      strcmp(name, EXECHELP_COMMANDS_SOCKET) == 0 ||
      strcmp(name, EXECHELP_EXECUTION_POLICY) == 0 ||
      strcmp(name, "container") == 0 ||
      strcmp(name, "LD_PRELOAD") == 0 ||
      strcmp(name, "LD_PRELOAD") == 0 ||
      strcmp(name, "PULSE_CLIENTCONFIG") == 0 ||
      strcmp(name, "QT_X11_NO_MITSHM") == 0 ||
      strcmp(name, "DBUS_SESSION_BUS_ADDRESS") == 0) {
      
    if (arg_debug || 1) //FIXME
      printf ("Setting environment variable %s to value '%s'\n", name, value);
    if (setenv(name, value, 1))
      errExit("setenv");
  }
}

void load_domain_env_from_chroot_proc() {
  uid_t uids[1] = {getuid()};
  PROCTAB *pt;
  proc_t *prt;
  char *name;
  char *value;
  int found_real = 0;

  if ((pt = openproc(PROC_FILLCOM | PROC_FILLENV | PROC_UID, uids, 1)) == NULL)
    errExit("openproc");

  // we scan through /proc until we find something else than dbus-run-session or dbus-daemon
  while (!found_real) {
    if ((prt = readproc(pt, NULL)) == NULL)
      break;

    if (prt->cmdline[0] && strncmp(prt->cmdline[0], "dbus-run-session", 16) && strncmp(prt->cmdline[0], "dbus-daemon", 11)) {
      found_real = 1;
      if (arg_debug)
        printf ("Copying the environment variables of the original firejail client, identified by pid %d inside the sandbox\n", prt->tid);

      for (int i=0; prt->environ[i]; i++) {
        name = prt->environ[i];
        value = strchr(name, '=');
        if (value) {
          *value = 0;
          value += 1;
          load_relevant_env_variable(name, value);
        }
      }
    }

    freeproc(prt);
  }

  closeproc(pt);

}

int firejail_setenv(const char *name, const char *value, int overwrite) {
  if (!name || !value) {
    errno = EINVAL;
    return -1;
  }

  FILE *domain_env = get_domain_env_w(sandbox_pid, 0);
  int exists = 0;

  // set the environment variable first
  int set = setenv(name, value, overwrite) == 0;

  // if it worked and we have a file to write to, cache the environment
  if (set && domain_env) {
    // try and find an identical line if we mustn't overwrite
    if (!overwrite) {
      char *buffer = NULL;
      size_t n = 0;
      ssize_t linelen = 0;
      size_t len = strlen(name);
      while (!exists && (linelen = getline(&buffer, &n, domain_env)) != -1) {
        if (strncmp(name, buffer, len) == 0)
          exists = 1;
        free(buffer);
        buffer = NULL;
        n = 0;
      }
    }

    // do the write
    if (overwrite || !exists) {
        char *line;
        if (asprintf(&line, "%s=%s\n", name, value) == -1)
          errExit("asprintf");

        errno = 0;
        size_t written = fwrite(line, sizeof(char), strlen(line), domain_env);
        if (errno)
          errExit("fwrite");

        free(line);
    }
  }
}

void firejail_setenv_finalize(void) {
  get_domain_env_w(sandbox_pid, 1);
}

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
    if (firejail_setenv(EXECHELP_SANDBOX_TYPE_ENV, "protected", 1) < 0)
      errExit("setenv");
  } else {
    exechelp_logv("firejail", "Declaring sandbox as untrusted\n");
    if (firejail_setenv(EXECHELP_SANDBOX_TYPE_ENV, "untrusted", 1) < 0)
      errExit("setenv");
  }

  if (cfg.hostname) {
    if (firejail_setenv(EXECHELP_SANDBOX_NAME_ENV, cfg.hostname, 1) < 0)
      errExit("setenv");
  }

  if (cfg.hostname && cfg.lock_workspace) {
    if (firejail_setenv(EXECHELP_SANDBOX_LOCK_WS_ENV, cfg.hostname, 1) < 0)
      errExit("setenv");
  }

  if (arg_debug_eh) {
    char *level;
    if (asprintf(&level, "%d", arg_debug) == -1)
      errExit("asprintf");
    if (firejail_setenv(EXECHELP_DEBUG_ENV, level, 1) < 0)
      errExit("setenv");
    free(level);
  }
}

#define _EH_HASH_STR_LEN 12
static char *exechelp_gen_socket_name(const char *socket_name, const pid_t pid) {
  char str[_EH_HASH_STR_LEN + 1];
  str[_EH_HASH_STR_LEN] = '\0';
  int available, i;

  // get a file path of the form "/run/firejail/<pid>-commands-<rand>
  char *path = NULL;
  int fd;

  // just in case there are leftovers in there...
  available = 0;
  while (!available) {
    // generate a random string
    for(i = 0; i < 12; i++)
      sprintf(str + i, "%x", rand() % 16);

    // generate a file path
    free(path);
    if (asprintf(&path, "%s/%d/%s-%s.sock", EXECHELP_RUN_DIR, pid, socket_name, str) == -1)
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
  exechelp_build_run_user_dir(sandbox_pid);

  cmdsocketpath = exechelp_gen_socket_name("commands", sandbox_pid);
  if (!cmdsocketpath)
    errExit("exechelp_gen_socket_name");
  exechelp_set_socket_env_manually(cmdsocketpath);
  if (arg_debug)
    printf("Created a socket at '%s' for client to inform fireexecd of commands it wants to run\n", cmdsocketpath);
}

void exechelp_set_socket_env_from_pid(const pid_t pid) {
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
      exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error when reading runtime directory %s to find child process's socket: %s\n", socketdir, strerror(errno));
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
    exechelp_logerrv("firejail", FIREJAIL_ERROR, "Could not find any socket in child process's runtime directory, aborting\n");
    exit(-1);
  }
}

void exechelp_set_socket_env_manually(char *cmdsocketpath) {
  if(!cmdsocketpath)
    return;

  if (firejail_setenv(EXECHELP_COMMANDS_SOCKET, cmdsocketpath, 1) < 0)
    errExit("setenv");
}

void exechelp_register_socket(void) {
  if (!cmdsocketpath) {
    exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: exechelp_register_socket called before its socket was initialised, check the firejail source code.\n");
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
    exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: no execution helper daemon is running. Please run 'fireexecd' prior to running firejail, or use the '--disable-helper' option to run firejail without execution helper (can help to circumvent bugs).\n");
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
        exechelp_logerrv("firejail", FIREJAIL_WARNING, "Error: the execution helper daemon did not allow us to proceed with sending commands for remote execution, setting a client policy that allows all executions internally.\n");
        if (asprintf(&pol, "%d", SANDBOX_ITSELF) == -1)
          errExit("asprintf");
      } else {
        if (asprintf(&pol, "%d", EXECHELP_DEFAULT_POLICY) == -1)
          errExit("asprintf");
      }
      if (firejail_setenv(EXECHELP_EXECUTION_POLICY, pol, 1) == -1)
        errExit("setenv");

      free(pol);
  } else {
      if (received < 0)
        errExit("recv");
      else {
        exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: the execution helper daemon should be running but sent data we could not receive. 'fireexecd' might have crashed. Use the '--disable-helper' option to run firejail without execution helper (can help to circumvent bugs).\n");
        exit(1);
      }
  }

  close(s);
}
