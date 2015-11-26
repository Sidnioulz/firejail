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
#include <libgen.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

static int alive = 1;
static void sig_handler(int signo)
{
  DBGENTER(-1, "sig_handler");
  if (signo == SIGTERM)
    alive = 0;
  DBGLEAVE(-1, "sig_handler");
}

static void command_execute(fireexecd_client_t *cli, const char *command, char *argv[]) {
  DBGENTER(cli?cli->pid:-1, "command_execute");

  pid_t id = fork();

  if (id == -1) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to fork when attempting to execute the '%s' command (error: %s)\n", cli->pid, command, strerror(errno));
    DBGLEAVE(cli?cli->pid:-1, "command_execute");
  } else if (id == 0) {
    // TODO permanently log
    if (execvpe(command, argv, environ) == -1) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to execute the '%s' command (error: %s)\n", cli->pid, command, strerror(errno));
    }
  }
  DBGLEAVE(cli?cli->pid:-1, "command_execute");
}

static int execd_is_catalogued_generic_run(fireexecd_client_t *cli, const char *catalogue, const char *target) {
  DBGENTER(cli?cli->pid:-1, "execd_is_catalogued_generic_run");
  if (!target || !cli || !catalogue) {
    DBGLEAVE(cli?cli->pid:-1, "execd_is_catalogued_generic_run");
    return 0;
  }

  char *path = NULL;
  if (asprintf(&path, "%s/%d-%s", EXECHELP_RUN_DIR, cli->pid, catalogue) == -1) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
    DBGLEAVE(cli?cli->pid:-1, "execd_is_catalogued_generic_run");
    return 0;
  }

  char *list = exechelp_read_list_from_file(path);
  // free(path);
  if (!list) {
    DBGLEAVE(cli?cli->pid:-1, "execd_is_catalogued_generic_run");
    return 0;
  }

  DBGLEAVE(cli?cli->pid:-1, "execd_is_catalogued_generic_run");
  return string_in_list(list, target);
}

static void parse_selected_profile(fireexecd_client_t *cli, char *selection, char **profile, int *sandbox) {
  DBGENTER(cli?cli->pid:-1, "parse_selected_profile");
  if (strcmp(EXECHELP_PROFILE_ANY, selection) == 0) {
    *profile = strdup(DEFAULT_USER_PROFILE);
    *sandbox = 1;
  } else if (strcmp(EXECHELP_PROFILE_NONE, selection) == 0) {
    *profile = NULL;
    *sandbox = 0;
  } else {
    *profile = strdup(selection);
    *sandbox = 1;
  }

  if(*sandbox)
    DBGOUT("[%d]\tINFO:  Client's next command will be run sandboxed with profile '%s'\n", cli?cli->pid:-1, *profile);
  else
    DBGOUT("[%d]\tINFO:  Client's next command will be run unsandboxed\n", cli?cli->pid:-1);

  DBGLEAVE(cli?cli->pid:-1, "parse_selected_profile");
}

static int select_profile(fireexecd_client_t *cli, char *app_profiles, char *file_profiles, char **profile, int *sandbox) {
  DBGENTER(cli?cli->pid:-1, "select_profile");
  if (!profile || !sandbox) {
    DBGLEAVE(cli?cli->pid:-1, "select_profile");
    return -1;
  }

  if (!app_profiles && !file_profiles) {
    DBGOUT("[%d]\tINFO:  Client's next command cannot be run sandboxed since no profile was given in the policy\n", cli?cli->pid:-1);
    *sandbox = 0;
    *profile = NULL;
    DBGLEAVE(cli?cli->pid:-1, "select_profile");
    return 0;
  }

  if (!app_profiles) {
    DBGOUT("[%d]\tINFO:  Client's next command will be sandboxed based on the protected files policy\n", cli?cli->pid:-1);
    split_comma(file_profiles);
    parse_selected_profile(cli, file_profiles, profile, sandbox);
    DBGLEAVE(cli?cli->pid:-1, "select_profile");
    return 0;
  }

  if (!file_profiles) {
    DBGOUT("[%d]\tINFO:  Client's next command will be sandboxed based on the protected applications policy\n", cli?cli->pid:-1);
    split_comma(app_profiles);
    parse_selected_profile(cli, app_profiles, profile, sandbox);
    DBGLEAVE(cli?cli->pid:-1, "select_profile");
    return 0;
  }

  // when both profiles are set
  char *app_ptr = app_profiles;
  char *file_ptr = file_profiles;
  DBGOUT("[%d]\tINFO:  Client's next command will be sandboxed based on a mixture of protected files and applications policies\n", cli?cli->pid:-1);

  while(app_ptr) {
    file_ptr = file_profiles;

    if (strcmp_comma(EXECHELP_PROFILE_ANY, app_ptr) == 0) {
      split_comma(file_profiles);
      parse_selected_profile(cli, file_profiles, profile, sandbox);
      return 0;
    }

    while(file_ptr) {
      if ((strcmp_comma(app_ptr, file_ptr) == 0) || (strcmp_comma(EXECHELP_PROFILE_ANY, file_ptr) == 0)) {
        split_comma(app_profiles);
        parse_selected_profile(cli, app_profiles, profile, sandbox);
        return 0;
      }

      file_ptr = strchr(file_ptr, ',');
    }

    app_ptr = strchr(app_ptr, ',');
  }

  *profile = NULL;
  *sandbox = 0;
  DBGLEAVE(cli?cli->pid:-1, "select_profile");
  return 0;
}

static char *get_profiles_for_files(char *argv[], const ExecHelpExecutionPolicy *decisions) {
  //TODO proper matching..
  /*
  
  char **current_list;
  
  for each file () {
    get profile...
    if current_list is NULL, current_list = profile
    else intersect current_list and profile
      if profile contains *, and current_list doesnt, expand current_list into that position
      if current_list contains *, and profile doesnt, expand profile into that position
      if both contain *, intersect the priors, add *
      else just intersect both whole lists
  }
  
  join into a comma separated list
  
  */

  return strdup(EXECHELP_PROFILE_ANY);
}

static char *get_profiles_for_new_app (const char *appname) {
  char *result = NULL;

  if (arg_debug)
		printf("Looking up user's config directory for a list of protected apps\n");

  char *filepath;
  FILE *fp;

  // first check for a config file in ~
  // TODO: add a cfg structure, a --home flag and update the manual
  struct passwd *pw = getpwuid(getuid());
  if (pw) {
    const char *homedir = pw->pw_dir;

	  if (asprintf(&filepath, "%s/.config/firejail/%s", homedir, EXECHELP_PROTECTED_APPS_BIN) == -1)
		  errExit("asprintf");
	  fp = fopen(EXECHELP_PROTECTED_APPS_BIN, "rb");
	  free(filepath);
  }

  if (fp == NULL) {
    if (arg_debug)
		  printf("Looking up /etc/firejail for a list of protected apps\n");
    filepath;
	  if (asprintf(&filepath, "/etc/firejail/%s", EXECHELP_PROTECTED_APPS_BIN) == -1)
		  errExit("asprintf");
  	fp = fopen(filepath, "rb");
  	free(filepath);
  }

	if (fp == NULL) {
		fprintf(stderr, "Warning: could not find any file listing protected apps\n");
	  return NULL;
	}

  if (arg_debug)
  	printf("Reading the list of protected apps from a file\n");

	// read the file line by line
	char buf[EXECHELP_POLICY_LINE_MAX_READ + 1], process[EXECHELP_POLICY_LINE_MAX_READ + 1], proflist[EXECHELP_POLICY_LINE_MAX_READ + 1];
	int lineno = 0;
	while (fgets(buf, EXECHELP_POLICY_LINE_MAX_READ, fp)) {
		++lineno;

    // get current line's process
    int pathlen = snprintf(process, EXECHELP_POLICY_LINE_MAX_READ, "%s", buf);
    if (pathlen == -1)
      errExit("snprintf");
    else if (pathlen >= EXECHELP_POLICY_LINE_MAX_READ) {
      fprintf(stderr, "Error: line %d of protected-apps.bin in malformed, the process path is longer than %d characters and the profile list cannot be read\n", lineno, EXECHELP_POLICY_LINE_MAX_READ);
      errno = ENAMETOOLONG;
      errExit("snprintf");
    }

    // if that process matches the command we're looking for, return the next profile name
    if (strcmp(appname, process) == 0) {
      int proflen = snprintf(proflist, EXECHELP_POLICY_LINE_MAX_READ - pathlen - 1, "%s", buf + pathlen + 1);
      if (proflen == -1)
        errExit("snprintf");
      else if (*(buf + pathlen + proflen) != '\n') {
        fprintf(stderr, "Error: line %d of protected-apps.bin in malformed, the line is longer than %d characters and the profile list cannot be read\n", lineno, EXECHELP_POLICY_LINE_MAX_READ);
        errno = ENAMETOOLONG;
        errExit("snprintf");
      }

      // remove the trailing \n before splitting the string
      proflist[proflen-1] = '\0';

      return strdup(proflist);
    }
	}

	fclose(fp);
  return NULL;
}

static void client_command_process(fireexecd_client_t *cli, char *msg, ssize_t len) {
  DBGENTER(cli?cli->pid:-1, "client_command_process");
  if (!cli) {
    DBGERR("[n/a]\t\e[01;40;101mERROR:\e[0;0m cannot process a command for a NULL client, aborting\n");
    DBGLEAVE(-1, "client_command_process");
    return;
  }

  DBGOUT("[%d]\tINFO:  Processing command '%s' on behalf of the client...\n", cli->pid, msg);

  // the command is directly at the start of the buffer
  char *command = msg;
  if (!command) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m cannot process a NULL command, aborting\n", cli->pid);
    DBGLEAVE(cli?cli->pid:-1, "client_command_process");
    return;
  }

  ssize_t browsed = strlen(msg) + 1;
  if (browsed > len) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m command is longer than the advertised message length, aborting (client might have a corrupted memory)\n", cli->pid);
    DBGLEAVE(cli?cli->pid:-1, "client_command_process");
    return;
  }

  // browse the string to all the length has been read, to extract parameters
  char **argv = malloc(sizeof(char*) * 2);
  argv[0] = command;
  argv[1] = NULL;
  size_t argc = 0;
  char *ptr = msg + browsed;
  while(browsed < len) {
    argc++;
    argv = realloc(argv, sizeof(char*) * (argc+2));
    argv[argc] = ptr;
    argv[argc+1] = NULL;
    if (arg_debug >= 2)
      DBGOUT("[%d]\tINFO:  Parameter %ld: '%s'...\n", cli->pid, argc, ptr);
  
    browsed += strlen(ptr) + 1;
    ptr = msg + browsed;
  }
  
  int associated = execd_is_catalogued_generic_run (cli, EXECHELP_LINKED_APPS, command);
  int protected  = execd_is_catalogued_generic_run (cli, EXECHELP_PROTECTED_APPS, command);

  int allowed_as_linked = (cli->pol & LINKED_APP) && associated;
  int allowed_as_unspec = (cli->pol & UNSPECIFIED) && !associated && !protected;
  int allowed_as_protected = (cli->pol & SANDBOX_PROTECTED) && !associated && protected;
  int protected_file = 0;
  char *whitelist_files = NULL;
  ExecHelpExecutionPolicy *decisions = NULL;

  char *reason = NULL;
  // the binary was not executable for our client, find out why
  if (!allowed_as_linked && !allowed_as_unspec && !allowed_as_protected) {
    if (associated) {
      DBGOUT("[%d]\tINFO:  Child process delegated command call because it cannot execute its own linked applications (~LINKED_APP policy)\n", cli->pid);
      reason = strdup("The sandboxed process tried to execute an application linked to its own package, but isn't allowed to do so.");
    } else if (protected) {
      DBGOUT("[%d]\tINFO:  Child process delegated command call because it cannot execute a protected application (~SANDBOX_PROTECTED policy)\n", cli->pid);
      reason = strdup("The sandboxed process tried to execute a protected application, but isn't allowed to do so.");
    } else {
      DBGOUT("[%d]\tINFO:  Child process delegated command call because it cannot open arbitrary applications (~UNSPECIFIED policy)\n", cli->pid);
      reason = strdup("The sandboxed process tried to execute an application that isn't linked to its own package, but isn't allowed to do so.");
    }
  }

  // check for additional restrictions on parameters
  char *path = NULL;
  if (asprintf(&path, "%s/%d-%s", EXECHELP_RUN_DIR, cli->pid, EXECHELP_PROTECTED_FILES) == -1) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
    DBGLEAVE(cli?cli->pid:-1, "client_command_process");
    return;
  }
  decisions = exechelp_targets_sandbox_protected_file(path, command, argv, 0, &whitelist_files);
  free(path);
  
  ExecHelpExecutionPolicy *iter = decisions + 1;
  while (*iter) {
    protected_file |= !(*iter & (LINKED_APP | UNSPECIFIED));
    ++iter;
  }

  // we're facing a file restriction, must identify which files are forbidden
  if (protected_file) {
    DBGOUT("[%d]\tINFO:  Child process delegated command call because of protected parameters\n", cli->pid);
    iter = decisions + 1;
    int i = 1;

    // create a placeholder for the string we'll use to display debug information
    size_t lenleft = EXECHELP_COMMAND_MAXLEN + argc*2; // enough for command length + separators
    char *tmp = malloc(sizeof(char) * (lenleft));
    if (!tmp) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call malloc() (error: %s)\n", cli->pid, strerror(errno));
      DBGLEAVE(cli?cli->pid:-1, "client_command_process");
      return;
    } else
      tmp[0] = '\0';

    while (*iter) {
      if (!(*iter & (LINKED_APP | UNSPECIFIED))) {
        // update the string used for debugging...
        if (tmp[0] != '\0') {
          strncat(tmp, ", ", lenleft-1);
          lenleft -= 2;
        }
        strncat(tmp, argv[i], lenleft-1);
        lenleft -= strlen(argv[i]);
      }
      ++iter;
      ++i;
    }

    if (asprintf(&reason, "The sandboxed process was not allowed to open the following protected files: %s", tmp) == -1) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
      DBGLEAVE(cli?cli->pid:-1, "client_command_process");
      return;
    }

    free(tmp);
  } else {
    DBGOUT("[%d]\tINFO:  Child process delegated command call but no justification could be backtraced by fireexecd (this is a bug in fireexecd or libexechelper)\n", cli->pid);
    reason = strdup("The sandboxed process was not allowed to execute an application or open a file, but the reason could not be determined.");
  }

  // show a notification, the brutal way...
  char *clientcmd = pids[cli->pid].cmd;
  if (!clientcmd) {
    if (asprintf(&clientcmd, "Process %d", cli->pid) == -1) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
      DBGLEAVE(cli?cli->pid:-1, "client_command_process");
      return;
    }
  } else
    clientcmd = strdup(clientcmd);

  char *notification;
  if (asprintf(&notification, "notify-send -i firejail-protect \"%s's system call was delegated to the sandbox\" \"%s\"", clientcmd, reason) == -1) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
      DBGLEAVE(cli?cli->pid:-1, "client_command_process");
      return;
  } else {
    int ret = system(notification);
    free(notification);
  }
  free(clientcmd);

  char *app_profiles = NULL;
  char *file_profiles = NULL;

  // identify the necessary profile for protected apps
  if (protected) {
    app_profiles = get_profiles_for_new_app(command);
    if (!app_profiles) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to find available sandbox profiles for protected application '%s'\n", cli->pid, command);
      DBGLEAVE(cli?cli->pid:-1, "client_command_process");
      return;
    } else
      DBGOUT("[%d]\tINFO:  Loaded available sandbox profiles for protected application '%s': '%s'\n", cli?cli->pid:-1, command, app_profiles);
  }

  if (protected_file) {
    file_profiles = get_profiles_for_files(argv, decisions);
    if (!file_profiles) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to find available sandbox profiles for protected files passed to command '%s'\n", cli->pid, command);
      DBGLEAVE(cli?cli->pid:-1, "client_command_process");
      return;
    } else
      DBGOUT("[%d]\tINFO:  Loaded available sandbox profiles for protected files passed to command '%s': '%s'\n", cli?cli->pid:-1, command, file_profiles);
  }

  char *profile = NULL;
  int sandbox = 0;

  if (select_profile(cli, app_profiles, file_profiles, &profile, &sandbox) == -1) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m could not select a sandbox profile to use for command '%s'\n", cli->pid, command);
    DBGLEAVE(cli?cli->pid:-1, "client_command_process");
    return;
  }

  free(app_profiles);
  free(file_profiles);

  if (sandbox) {
    DBGOUT("[%d]\tINFO:  Command '%s' will be executed in a sandbox...\n", cli?cli->pid:-1, command);
    char *sandboxcommand = "firejail";
    char **sandboxargv = malloc(sizeof(char *) * (argc+2 /* original argv size */ + 30 /* firejail and some options */));
    size_t index=0;
    size_t offset=0;
    sandboxargv[index++] = sandboxcommand;
    sandboxargv[index++] = "--helper";
    // if you add too many lines here, remember to allocate more memory
    if (arg_debug) {
      sandboxargv[index++] = "--debug";
    }
    if (profile) {
      char *profiletxt;
      if (asprintf(&profiletxt, "--profile=/etc/firejail/%s.profile", profile) == -1) {
        DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
        DBGLEAVE(cli?cli->pid:-1, "client_command_process");
        return;
      }
      sandboxargv[index++] = profiletxt;
    }

    if(protected) {
      char *white = NULL;
      if(asprintf(&white, "--whitelist-apps=%s", command) == -1) {
        DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
        DBGLEAVE(cli?cli->pid:-1, "client_command_process");
        return;
      }
      sandboxargv[index++] = white;
    }
    if(protected_file) {
      char *white = NULL;
      if (!whitelist_files) {
        DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to generate a list of white-listed files, cannot proceed as it could cause firejail to recursively call itself and deplete the system's resources. Aborting.\n", cli->pid);
        DBGLEAVE(cli?cli->pid:-1, "client_command_process");
        return;
      } else if(asprintf(&white, "--whitelist-files=%s", whitelist_files) == -1) {
        DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
        DBGLEAVE(cli?cli->pid:-1, "client_command_process");
        return;
      }
      free(whitelist_files);
      sandboxargv[index++] = white;
    }

    offset = index;
    while(argv[index-offset]) {
      sandboxargv[index] = argv[index-offset];
      ++index;
    }
    sandboxargv[index] = NULL;

    // Debugging
    if (arg_debug)
      DBGOUT("[%d]\tINFO:  Now executing command '%s' in sandbox with profile '%s'...\n", cli->pid, command, profile);
    if (arg_debug >= 2) {
      int z;
      for (z=0; sandboxargv[z]; z++)
        DBGOUT("[%d]\tINFO:  \targv[%d]: '%s'\n", cli->pid, z, sandboxargv[z]);
    }

    command_execute(cli, sandboxcommand, sandboxargv);
  } else {
    //TODO dbg that we run unsandboxed

    int z;
    for (z=0; argv[z]; z++)
      printf("\t%s****\n", argv[z]);
    command_execute(cli, command, argv);
  }

  DBGLEAVE(cli?cli->pid:-1, "client_command_process");
}

void client_handle_real(fireexecd_client_t *cli) {
  DBGENTER(cli?cli->pid:-1, "client_handle_real");

  if (!cli) {
    DBGERR("[n/a]\t\e[01;40;101mERROR:\e[0;0m cannot handle a NULL client, aborting\n");
    DBGLEAVE(cli?cli->pid:-1, "client_handle_real");
    exit(-1);
  }

  if (!cli->cmdpath) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m cannot handle client without a command socket, aborting\n", cli->pid);
    DBGLEAVE(cli?cli->pid:-1, "client_handle_real");
    exit(-1);
  }

  if (signal(SIGTERM, sig_handler) == SIG_ERR)
    DBGERR("[%d]\t\e[02;30;103mWARN:\e[0;0m  could not install a signal handler for SIGTERM, may not terminate cleanly\n", cli->pid);

  // find out the identity we ought to use
  char *dirname = strdup(cli->cmdpath);
  if (!dirname) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call strdup() (error: %s)\n", cli->pid, strerror(errno));
    DBGLEAVE(cli?cli->pid:-1, "client_handle_real");
    exit(-1);
  }
  char *last = strrchr(dirname, '/');
  *last = '\0';

  struct stat s;
  if(stat(dirname, &s) == -1) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m could not retrieve the identity of the client's socket directory owner (error: %s)\n", cli->pid, strerror(errno));
    DBGLEAVE(cli?cli->pid:-1, "client_handle_real");
    exit(-1);
  }

  // get the command socket
  if (arg_debug >= 2)
    DBGOUT("[%d]\tINFO:  Opening socket '%s' to communicate with client...\n", cli->pid, cli->cmdpath);
  
  int cmdsock, clisock;
  struct sockaddr_un local, remote;
  socklen_t t;

  if ((cmdsock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m could not create a client command socket (error: %s)\n", cli->pid, strerror(errno));
    DBGLEAVE(cli?cli->pid:-1, "client_handle_real");
    exit(-1);
  }

  local.sun_family = AF_UNIX;
  strcpy(local.sun_path, cli->cmdpath);
  unlink(local.sun_path); // only one handler!
  size_t len = strlen(local.sun_path) + sizeof(local.sun_family);
  if (bind(cmdsock, (struct sockaddr *)&local, len) == -1) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m could not bind the client command socket to the path given when registering (error: %s)\n", cli->pid, strerror(errno));
    DBGLEAVE(cli?cli->pid:-1, "client_handle_real");
    exit(-1);
  }

  if(chown(cli->cmdpath, s.st_uid, s.st_gid) == -1) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m could not align the ownership of client's command socket with client's identity (error: %s)\n", cli->pid, strerror(errno));
    DBGLEAVE(cli?cli->pid:-1, "client_handle_real");
    exit(-1);
  }
  free(dirname);

  if (listen(cmdsock, SOMAXCONN) == -1) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m could not open the client command socket to listen for new connections (error: %s)\n", cli->pid, strerror(errno));
    DBGLEAVE(cli?cli->pid:-1, "client_handle_real");
    exit(-1);
  }
  
  fireexecd_drop_privs();
  DBGOUT("[%d]\tINFO:  Dropped root privileges\n", cli->pid);

  char msg[EXECHELP_COMMAND_MAXLEN + 1];
  msg[EXECHELP_COMMAND_MAXLEN] = '\0';

  while(alive) {
    t = sizeof(remote);
    DBGOUT("[%d]\tINFO:  Waiting for the next command...\n", cli->pid);
    if ((clisock = accept(cmdsock, (struct sockaddr *)&remote, &t)) == -1) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to accept a new connection from client, continuing to listen for new connections (error: %s)\n", cli->pid, strerror(errno));
      continue;
    }

    ssize_t n = recv(clisock, msg, EXECHELP_COMMAND_MAXLEN, 0);
    if (n < 0) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to process a packet sent by the client, continuing to listen for new connections (error: %s)\n", cli->pid, strerror(errno));
      close(clisock);
      continue;
    }
    msg[n] = '\0';
    if (arg_debug >= 2)
      DBGOUT("[%d]\tINFO:  Client sent a command packet, of length %ld: '%s'\n", cli->pid, n, msg);
    else
      DBGOUT("[%d]\tINFO:  Client sent a command packet, processing it...\n", cli->pid);

    client_command_process(cli, msg, n);
    close(clisock);
  }

  // close socket and exit
  close(cmdsock);
  DBGOUT("[%d]\tINFO:  Done handling the sandboxed client, leaving\n", cli->pid);
  DBGLEAVE(cli?cli->pid:-1, "client_handle_real");
  exit(0);
}

