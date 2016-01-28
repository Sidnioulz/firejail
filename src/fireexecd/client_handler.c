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
#include <stdlib.h>
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

static int command_execute(fireexecd_client_t *cli, const char *command, char *argv[]) {
  DBGENTER(cli?cli->pid:-1, "command_execute");

  pid_t id = exechelp_fork();

  if (id == -1) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to fork when attempting to execute the '%s' command (error: %s)\n", cli->pid, command, strerror(errno));
    DBGLEAVE(cli?cli->pid:-1, "command_execute");
    return -1;
  } else if (id == 0) {
    if (execvpe(command, argv, environ) == -1) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to execute the '%s' command (error: %s)\n", cli->pid, command, strerror(errno));
      DBGLEAVE(cli?cli->pid:-1, "command_execute");
      return -1;
    }
  }

  DBGLEAVE(cli?cli->pid:-1, "command_execute");
  return 0;
}

int client_execute_sandboxed(fireexecd_client_t *cli,
                             const char *command,
                             char *argv[], int argc,
                             const char *profile,
                             const char *name,
                             int is_protected_app,
                             int has_protected_files,
                             char **files_to_whitelist) {
  DBGENTER(cli?cli->pid:-1, "client_execute_sandboxed");

  DBGOUT("[%d]\tINFO:  Command '%s' will be executed in a sandbox with profile '%s' and name '%s'\n", cli?cli->pid:-1, command, profile, name);
  size_t size = (argc+2 /* original argv size */ + 30 /* firejail and some options */);
  char **sandboxargv = malloc(sizeof(char *) * size);
  size_t index=0;
  size_t offset=0;
  sandboxargv[index++] = strdup("firejail");
  sandboxargv[index++] = strdup("--helper");
  // if you add too many lines here, remember to allocate more memory
  char *dbg_env = getenv(EXECHELP_DEBUG_ENV);
  errno = 0;
  int dbg = strtol(dbg_env? dbg_env : "0", NULL, 10);
  if (!errno && dbg);
    sandboxargv[index++] = strdup("--debug");

  if (profile && strcmp(profile, EXECHELP_PROFILE_ANY)) {
    char *profiletxt;
    if (asprintf(&profiletxt, "--profile=/etc/firejail/%s.profile", profile) == -1) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
      DBGLEAVE(cli?cli->pid:-1, "client_execute_sandboxed");
      return -1;
    }
    sandboxargv[index++] = profiletxt;
  }

  if (is_protected_app) {
    char *white = NULL;
    if(asprintf(&white, "--whitelist-apps=\"%s\"", command) == -1) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
      DBGLEAVE(cli?cli->pid:-1, "client_execute_sandboxed");
      return -1;
    }
    sandboxargv[index++] = white;
  }

  if (has_protected_files) {
    char *white = NULL, *tmp = NULL;
    if (!files_to_whitelist) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to generate a list of white-listed files, cannot proceed as it could cause firejail to recursively call itself and deplete the system's resources. Aborting.\n", cli->pid);
      DBGLEAVE(cli?cli->pid:-1, "client_execute_sandboxed");
      return -1;
    }

    if (!string_list_is_empty(files_to_whitelist)) {
      tmp = string_list_flatten(files_to_whitelist, ",");
       if(asprintf(&white, "--whitelist-files=\"%s\"", tmp) == -1) {
        DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
        DBGLEAVE(cli?cli->pid:-1, "client_execute_sandboxed");
        return -1;
      }
      free(tmp);
      sandboxargv[index++] = white;
    }
  }

  if (name) {
    char *nametxt;
    if (asprintf(&nametxt, "--join=%s", name) == -1) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
      DBGLEAVE(cli?cli->pid:-1, "client_execute_sandboxed");
      return -1;
    }
    sandboxargv[index++] = nametxt;
  }

  offset = index;
  while(argv[index-offset]) {
    sandboxargv[index] = strdup(argv[index-offset]);
    ++index;
  }
  sandboxargv[index] = NULL;

  // Debugging
  if (arg_debug) {
    int z;
    for (z=0; sandboxargv[z]; z++)
      DBGOUT("[%d]\tINFO:  \targv[%d]: '%s'\n", cli->pid, z, sandboxargv[z]);
  }

  int ret = command_execute(cli, "firejail", sandboxargv);
  string_list_free(&sandboxargv);

  DBGLEAVE(cli?cli->pid:-1, "client_execute_sandboxed");
  return ret;
}

int client_execute_unsandboxed(fireexecd_client_t *cli,
                               const char *command,
                               char *argv[]) {
  DBGENTER(cli?cli->pid:-1, "client_execute_unsandboxed");

  DBGOUT("[%d]\tINFO:  Command '%s' will be executed unsandboxed...\n", cli?cli->pid:-1, command);
  // Debugging
  if (arg_debug) {
    int z;
    for (z=0; argv[z]; z++)
      DBGOUT("[%d]\tINFO:  \targv[%d]: '%s'\n", cli->pid, z, argv[z]);
  }

  int ret = command_execute(cli, command, argv);

  DBGLEAVE(cli?cli->pid:-1, "client_execute_sandboxed");
  return ret;
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
  if (firejail_strcmp(EXECHELP_PROFILE_ANY, selection) == 0) {
    *profile = strdup(DEFAULT_USER_PROFILE);
    *sandbox = 1;
  } else if (firejail_strcmp(EXECHELP_PROFILE_NONE, selection) == 0) {
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

static int validate_handler(fireexecd_client_t *cli,
                            const char *command, const char *command_profile,
                            ExecHelpProtectedFileHandler *h,
                            ExecHelpProtectedFileHandler **backup_command,
                            ExecHelpProtectedFileHandler **backup_valid_handler) {
  DBGENTER(cli?cli->pid:-1, "validate_handler");

  if ((!backup_command) || (!backup_valid_handler)) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m validate_handler() needs placeholders for partially validated (handler; profile) tuples.\n", cli->pid);
    DBGLEAVE(cli?cli->pid:-1, "validate_handler");
    return 0;
  }

  if (!h) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m validate_handler() called without a pre-matched (handler; profile) tuple to validate.\n", cli->pid);
    DBGLEAVE(cli?cli->pid:-1, "validate_handler");
    return 0;
  }

  // check if we're allowed to execute the file's profile
  char *realcommand = exechelp_resolve_executable_path(command);
  char *realhandler = exechelp_resolve_executable_path(h->handler_path);

  if ((firejail_strcmp(realcommand, realhandler) == 0) || (firejail_strcmp(EXECHELP_COMMAND_ANY, h->handler_path) == 0)) {
    free(realcommand);
    free(realhandler);
    DBGLEAVE(cli?cli->pid:-1, "validate_handler");
    return 1;
  }

  // calculate a backup profile, which will be used to propose the user the most fitting choices of profiles
  if (!*backup_command && realcommand && command_profile)
    *backup_command = protected_files_handler_new(strdup(realcommand), strdup(command_profile));
  if (!*backup_valid_handler)
    *backup_valid_handler = protected_files_handler_copy(h, NULL);

  free(realcommand);
  free(realhandler);
  DBGLEAVE(cli?cli->pid:-1, "validate_handler");
  return 0;
}

static int select_profile(fireexecd_client_t *cli, const char *command,
                          char *app_profiles, ExecHelpList *file_profiles,
                          ExecHelpProtectedFileHandler **backup_command,
                          ExecHelpProtectedFileHandler **backup_valid_handler,
                          char **profile, int *sandbox) {
  int valid_handler = 0;
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

    ExecHelpList *file_ptr = file_profiles;
    while(file_ptr) {
      ExecHelpProtectedFileHandler *h = file_ptr->data;
      valid_handler = validate_handler(cli, command, NULL, h, backup_command, backup_valid_handler);

      if (valid_handler) {
        parse_selected_profile(cli, h? h->profile_name : NULL, profile, sandbox);
        DBGOUT("[%d]\tINFO: Client's next command '%s' is allowed to run all the passed protected files with profile '%s'\n", cli?cli->pid:-1, command, *sandbox ? *profile : "none, unsandboxed");
        DBGLEAVE(cli?cli->pid:-1, "select_profile");
        return 0;
      } else {
        DBGOUT("[%d]\tINFO: Client's next command '%s' is not allowed to run some of the passed protected files with profile entry (%s;%s), must spawn a dialog\n", cli?cli->pid:-1, command, h->handler_path, h->profile_name);
      }
      file_ptr = file_ptr->next;
    }

    DBGOUT("[%d]\tINFO: Client's next command '%s' is compatible with none of the available handlers, must spawn a dialog\n", cli?cli->pid:-1, command);
    return -1;
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
  while(app_ptr) {
    app_ptr = strchr(app_ptr, ','); if (app_ptr) app_ptr++;
  } app_ptr = app_profiles;
  ExecHelpList *file_ptr;
  DBGOUT("[%d]\tINFO:  Client's next command will be sandboxed based on a mixture of protected files and applications policies\n", cli?cli->pid:-1);

  while(app_ptr) {
    file_ptr = file_profiles;

    if (strcmp_comma(EXECHELP_PROFILE_ANY, app_ptr) == 0) {
      ExecHelpProtectedFileHandler *h = file_ptr->data;

      char *app_copy = strdup(app_ptr);
      split_comma(app_copy);
      valid_handler = validate_handler(cli, command, app_copy, h, backup_command, backup_valid_handler);
      free(app_copy);

      if (valid_handler) {
        parse_selected_profile(cli, h? h->profile_name : NULL, profile, sandbox);
        DBGLEAVE(cli?cli->pid:-1, "select_profile");
        return 0;
      }
    }

    while(file_ptr) {
      ExecHelpProtectedFileHandler *h = file_ptr->data;
      if ((strcmp_comma(app_ptr, h->profile_name) == 0) || (strcmp_comma(EXECHELP_PROFILE_ANY, h->profile_name) == 0)) {
        char *app_copy = strdup(app_ptr);
        split_comma(app_copy);
        valid_handler = validate_handler(cli, command, app_copy, h, backup_command, backup_valid_handler);
        free(app_copy);

        if (valid_handler) {
          split_comma(app_ptr);
          parse_selected_profile(cli, app_ptr, profile, sandbox);
          DBGOUT("[%d]\tINFO: Client's next command '%s' is allowed to run all the passed protected files with profile '%s'\n", cli?cli->pid:-1, command, *sandbox ? *profile : "none, unsandboxed");
          DBGLEAVE(cli?cli->pid:-1, "select_profile");
          return 0;
        }
      }

      file_ptr = file_ptr->next;
    }

    app_ptr = strchr(app_ptr, ','); if (app_ptr) app_ptr++;
  }

  // if we found an invalid handler, it means we had a match but it had the wrong
  // executable, ask for the user to make a choice instead of doing something arbitrary
  *profile = NULL;
  *sandbox = 0;
  
  DBGLEAVE(cli?cli->pid:-1, "select_profile");
  if (valid_handler)
    return 0;
  else
    return -1;
}

static void merge_profiles_for_files(fireexecd_client_t *cli, char **forbidden_files, ExecHelpList **profiles) {
  DBGENTER(cli?cli->pid:-1, "merge_profiles_for_files");
  if (!forbidden_files || !profiles || *profiles) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m must have protected files to parse before looking for compatible profiles\n", cli->pid);
    DBGLEAVE(cli?cli->pid:-1, "merge_profiles_for_files");
    return;
  }

  ExecHelpList *interp = NULL;
  ExecHelpList *lp = NULL;
  ExecHelpList *ip = NULL;

  for (int i = 0; forbidden_files[i]; i++) {
    ExecHelpList *prepend = NULL;
    ExecHelpList *local = protected_files_get_handlers_for_file(forbidden_files[i]);
    DBGOUT("[%d]\tINFO:  Looking for profiles for file '%s' that match other protected files' policy\n", cli?cli->pid:-1, forbidden_files[i]);

    // remove all from intersection that isn't in local
    if (i == 0) {
      interp = exechelp_list_copy_deep(local, (ExecHelpCopyFunc) protected_files_handler_copy, NULL);
    } else {
      for (lp = local; lp; lp = lp->next) {
//        printf ("doing %s:%s\n", ((ExecHelpProtectedFileHandler *)lp->data)->handler_path, ((ExecHelpProtectedFileHandler *)lp->data)->profile_name);

        for (ip = interp; ip; ip = ip->next) {
//          printf ("merging with %s:%s", ((ExecHelpProtectedFileHandler *)ip->data)->handler_path, ((ExecHelpProtectedFileHandler *)ip->data)->profile_name);
          ExecHelpProtectedFileHandler *merged = NULL;
          ExecHelpHandlerMergeResult result = protected_files_handlers_merge(lp->data, ip->data, &merged);

          if (result == HANDLER_IDENTICAL) {
            prepend = exechelp_list_prepend(prepend, protected_files_handler_copy(ip->data, NULL));
//            printf ("\tidentical\n");
            //do nothing
          } else if (result == HANDLER_USE_MERGED) {
//            printf ("\tmerged\n");
            prepend = exechelp_list_prepend(prepend, merged);
          } else if (result == HANDLER_UNMERGEABLE) {
//            printf ("\tunmergeable\n");
          }
//          else
//            printf ("\terror\n");
        } // end loop on intersection
      } // end loop on local

      exechelp_list_free_full(interp, (ExecHelpDestroyNotify) protected_files_handler_free);
      interp = prepend;
    }

    // debugging prints...
    if (arg_debug >= 1) {
      DBGOUT("[%d]\tINFO: File '%s' finished, remaining profiles:\n", cli?cli->pid:-1, forbidden_files[i]);
      int z = 0;
      for (ip = interp; ip; ip = ip->next) {
        ExecHelpProtectedFileHandler *h = ip->data;
        DBGOUT("[%d]\tINFO: \t\t[%d] (%s:%s)\n", cli?cli->pid:-1, z++, h->handler_path, h->profile_name);
      }
      if (z == 0)
        DBGOUT("[%d]\tINFO: \t\t(none)\n", cli?cli->pid:-1);
    }







  }

  // only a profile that matches all forbidden files ought to be used
  *profiles = interp;

  DBGOUT("[%d]\tINFO: The following profiles match all protected files' policies:\n", cli?cli->pid:-1);
  int i = 0;
  for (ExecHelpList *lp = interp; lp; lp = lp->next) {
    ExecHelpProtectedFileHandler *h = lp->data;
    DBGOUT("[%d]\tINFO: \t\t[%d] (%s:%s)\n", cli?cli->pid:-1, i++, h->handler_path, h->profile_name);
  }
  if (i == 0)
    DBGOUT("[%d]\tINFO: \t\t(none)\n", cli?cli->pid:-1);

  DBGLEAVE(cli?cli->pid:-1, "merge_profiles_for_files");
  return;
}

static char *get_profiles_for_new_app (fireexecd_client_t *cli, const char *appname) {
  DBGENTER(cli?cli->pid:-1, "get_profiles_for_new_app");

  char *result = NULL;
  char *filepath;
  FILE *fp;

  // first check for a config file in ~
  // TODO: add a cfg structure, a --home flag and update the manual
  struct passwd *pw = getpwuid(getuid());
  if (pw) {
    const char *homedir = pw->pw_dir;

	  if (asprintf(&filepath, "%s/.config/firejail/%s", homedir, EXECHELP_PROTECTED_APPS_BIN) == -1) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
      DBGLEAVE(cli?cli->pid:-1, "get_profiles_for_new_app");
      return NULL;
	  }

	  fp = fopen(EXECHELP_PROTECTED_APPS_BIN, "rb");
  	if (fp)
	    DBGOUT("[%d]\tINFO:  Looking up sandbox profiles for protected application '%s' via a user-specified config file\n", cli?cli->pid:-1, appname);
	  free(filepath);
  }

  if (fp == NULL) {
    if (arg_debug)
		  printf("Looking up /etc/firejail for a list of protected apps\n");
    filepath;
	  if (asprintf(&filepath, "/etc/firejail/%s", EXECHELP_PROTECTED_APPS_BIN) == -1) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
      DBGLEAVE(cli?cli->pid:-1, "get_profiles_for_new_app");
      return NULL;
	  }

  	fp = fopen(filepath, "rb");
  	if (fp)
      DBGOUT("[%d]\tINFO:  Looking up sandbox profiles for protected application '%s' via the system-wide config file\n", cli?cli->pid:-1, appname);
  	free(filepath);
  }

	if (fp == NULL) {
		fprintf(stderr, "Warning: could not find any file listing protected apps\n");
    DBGERR("[%d]\t\e[01;40;101mWARNING:\e[0;0m ould not find any file listing protected apps, protected apps won't be launchable\n", cli->pid);
    DBGLEAVE(cli?cli->pid:-1, "get_profiles_for_new_app");
	  return NULL;
	}

	// read the file line by line
	char buf[EXECHELP_POLICY_LINE_MAX_READ + 1];
	int lineno = 0;
	while (fgets(buf, EXECHELP_POLICY_LINE_MAX_READ, fp)) {
		++lineno;
		if (*buf == '#') // comment, ignore this line
		  continue;

    // remove the trailing "\n"
    buf[strlen(buf)-1] = '\0';

    // locate the separator, and replace it with a null character
    char *sep = NULL;
    if (!exechelp_parse_get_next_separator(buf, &sep, 1)) {
      if (arg_debug)
        fprintf(stderr, "Error: line %d is malformed, should be of the form: <path to file>///<profile name>:<path to handler>,<profile name>:<path to handler>,...\n", lineno);
	    return NULL;
    }

    /* at this stage buf represents the process, and sep represents the profile name */

    // if that process matches the appname we're looking for, return the next profile name
    if (firejail_strcmp(appname, buf) == 0) {
      DBGLEAVE(cli?cli->pid:-1, "get_profiles_for_new_app");
      return strdup(sep);
    }
	}

	fclose(fp);

  DBGOUT("[%d]\tINFO:  No profile was found for command '%s' (scanned %d lines)\n", cli?cli->pid:-1, appname, lineno);
  DBGLEAVE(cli?cli->pid:-1, "get_profiles_for_new_app");
  return NULL;
}

static int client_check_has_protected_files(fireexecd_client_t       *cli,
                                            const char               *command,
                                            char                     *argv[],
                                            char                   ***forbidden_files,
                                            char                   ***files_to_whitelist,
                                            char                    **reason) {
  DBGENTER(cli?cli->pid:-1, "client_check_has_protected_files");

  if (!forbidden_files || *forbidden_files || !files_to_whitelist || *files_to_whitelist) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m client_check_has_protected_files() needs placeholders for forbidden file lists.\n", cli->pid);
      DBGLEAVE(cli?cli->pid:-1, "client_check_has_protected_files");
    return 0;
  }

  if (!argv)
    return 0;

  char                    *policypath          = NULL;
  char                    *file_list           = NULL;
  int                      has_protected_files = 0;

  if (asprintf(&policypath, "%s/%d-%s", EXECHELP_RUN_DIR, cli->pid, EXECHELP_PROTECTED_FILES) == -1) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
    DBGLEAVE(cli?cli->pid:-1, "client_check_has_protected_files");
    return 0;
  }
  
  // get a list of protected files first
  char *managed = exechelp_read_list_from_file(policypath);
  free(policypath);
  if (!managed) {
    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m Could not find a list of sandbox-protected files for client\n", cli->pid);
    DBGLEAVE(cli?cli->pid:-1, "client_check_has_protected_files");
    return 0;
  }

  // loop through the arguments
  for (size_t i=1; argv[i]; ++i) {
    // get the canonical path of the current argument, if any
    char *real = exechelp_coreutils_realpath(argv[i]);

    //FIXME ignore whitelisted files here?

    char *prefix = NULL;
    if (exechelp_file_list_contains_path(managed, real, &prefix)) {
      has_protected_files++;
      printf ("\n\n\n\n\n\n now adding %s, and also its prefix %s\n\n\n\n\n", argv[i], prefix);
      string_list_append(forbidden_files, strdup(argv[i]));
      string_list_append(files_to_whitelist, prefix);
    }

    free(real);
  }

  // we're facing a file restriction, must identify which files are forbidden
  if (has_protected_files) {
    DBGOUT("[%d]\tINFO:  Child process delegated command call because of protected parameters\n", cli->pid);
    file_list = string_list_flatten(*forbidden_files, ", ");

    // add our reason to the existing one
    if (!*reason) {
      if (file_list) {
        if (asprintf(reason, "The sandboxed process was not allowed to open the following protected files: %s", file_list) == -1)
          DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
      } else {
         if ((*reason = strdup("The sandboxed process was not allowed to open some protected files.")) == NULL)
          DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call strdup() (error: %s)\n", cli->pid, strerror(errno));
      }
    }
    // create a new reason if only the protected files prevented the execution
    else {
      if (file_list) {
        if (asprintf(reason, "%s. Besides, the sandboxed process was not allowed to open the following protected files: %s", *reason, file_list) == -1)
          DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
      } else {
        if (asprintf(reason, "%s. Besides, the sandboxed process was not allowed to open some protected files.", *reason) == -1)
          DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call strdup() (error: %s)\n", cli->pid, strerror(errno));
      }
    }

    free(file_list);
  }

  DBGLEAVE(cli?cli->pid:-1, "client_check_has_protected_files");
  return has_protected_files;                              
}

static void command_process_real(fireexecd_client_t *cli,
                                 const char *caller_command,
                                 const char *command,
                                 char *argv[],
                                 const size_t argc) {
  DBGENTER(cli?cli->pid:-1, "client_command_process_real");

  /** first, identify the reasons for the command execution **/
  int is_linked_app        = execd_is_catalogued_generic_run (cli, EXECHELP_LINKED_APPS, command);
  int is_protected_app     = execd_is_catalogued_generic_run (cli, EXECHELP_PROTECTED_APPS, command);
  int allowed_as_linked    = (cli->pol & LINKED_APP) && is_linked_app;
  int allowed_as_unspec    = (cli->pol & UNSPECIFIED) && !is_linked_app && !is_protected_app;
  int allowed_as_protected = (cli->pol & SANDBOX_PROTECTED) && !is_linked_app && is_protected_app;
  int has_protected_files  = 0;

  char                   **forbidden_files    = NULL;
  char                   **files_to_whitelist = NULL;
  char                    *reason             = NULL;

  // the binary was not executable for our client, find out why
  if (!allowed_as_linked && !allowed_as_unspec && !allowed_as_protected) {
    if (is_linked_app) {
      DBGOUT("[%d]\tINFO:  Child process delegated command call because it cannot execute its own linked applications (~LINKED_APP policy)\n", cli->pid);
      if (asprintf(&reason, "The sandboxed process \"%s\" tried to execute an application linked to its own package, but isn't allowed to do so.", caller_command) == -1) {
        DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
        DBGLEAVE(cli?cli->pid:-1, "client_command_process_real");
        return;
      }
    } else if (is_protected_app) {
      DBGOUT("[%d]\tINFO:  Child process delegated command call because it cannot execute a protected application (~SANDBOX_PROTECTED policy)\n", cli->pid);
      if (asprintf(&reason, "The sandboxed process \"%s\" tried to execute \"%s\", which is a protected application.", caller_command, command) == -1) {
        DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
        DBGLEAVE(cli?cli->pid:-1, "client_command_process_real");
        return;
      }
    } else {
      DBGOUT("[%d]\tINFO:  Child process delegated command call because it cannot open arbitrary applications (~UNSPECIFIED policy)\n", cli->pid);
      if (asprintf(&reason, "The sandboxed process \"%s\" is only allowed to execute applications linked to its own package.", caller_command) == -1) {
        DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
        DBGLEAVE(cli?cli->pid:-1, "client_command_process_real");
        return;
      }
    }
  }

  // check for additional restrictions on parameters
  has_protected_files = client_check_has_protected_files(cli, command, argv, &forbidden_files, &files_to_whitelist, &reason);

  /** second, find a valid profile or ask the user to select a profile **/
  // if a reason is missing, ask the client what to do and exit
  if (!reason) {
    DBGOUT("[%d]\tINFO:  Child process delegated command call but no justification could be backtraced by fireexecd (this is a bug in fireexecd or libexechelper)\n", cli->pid);
    reason = strdup("The sandboxed process was not allowed to execute an application or open a file, but the reason could not be determined (this is a bug in the sandbox).");

    char *header;
    if (asprintf(&header, "%s's system call was denied by the sandbox", caller_command) == -1) {
        DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
        DBGLEAVE(cli?cli->pid:-1, "client_command_process_real");
        return;
    }
    client_notify(cli, "dialog-error", header, reason);
    free(header);
    client_dialog_run_without_reason(cli, caller_command, command, argv, argc, files_to_whitelist);

    string_list_free(&forbidden_files);
    string_list_free(&files_to_whitelist);
    DBGLEAVE(cli?cli->pid:-1, "client_command_process_real");
    return;
  }

  // identify the necessary profile for protected apps
  char         *app_profiles = NULL;
  ExecHelpList *file_profiles = NULL;

  if (is_protected_app) {
    app_profiles = get_profiles_for_new_app(cli, command);
    if (!app_profiles) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to find available sandbox profiles for protected application '%s'\n", cli->pid, command);
      DBGLEAVE(cli?cli->pid:-1, "client_command_process_real");
      return;
    } else
      DBGOUT("[%d]\tINFO:  Loaded available sandbox profiles for protected application '%s': '%s'\n", cli?cli->pid:-1, command, app_profiles);
  }

  if (has_protected_files) {
    merge_profiles_for_files(cli, forbidden_files, &file_profiles);
    if (!file_profiles) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to find available sandbox profiles for protected files passed to command '%s'\n", cli->pid, command);
      
      char *header;
      if (asprintf(&header, "A sandboxed app was prevented from opening '%s'", command) == -1) {
          DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
          string_list_free(&forbidden_files);
          string_list_free(&files_to_whitelist);
          free(reason);
          DBGLEAVE(cli?cli->pid:-1, "client_command_process_real");
          return;
      }
      client_notify(cli, "dialog-error", header, "The executed application is trying to open multiple incompatible protected files.");
      free(header);
      
      client_dialog_run_incompatible(cli, caller_command, command, argv, argc, forbidden_files, files_to_whitelist, NULL, NULL);
      string_list_free(&forbidden_files);
      string_list_free(&files_to_whitelist);
      free(reason);
      DBGLEAVE(cli?cli->pid:-1, "client_command_process_real");
      return;
    } else
      DBGOUT("[%d]\tINFO:  Loaded available sandbox profiles for protected files passed to command '%s'\n", cli?cli->pid:-1, command);
      DBGOUT("[%d]\tINFO:  Loaded available handlers for protected files passed to command '%s'\n", cli?cli->pid:-1, command);
  }

  char *profile = NULL;
  int sandbox = 0;
  ExecHelpProtectedFileHandler *backup_command = NULL;
  ExecHelpProtectedFileHandler *backup_valid_handler = NULL;

  if (select_profile(cli, command, app_profiles, file_profiles, &backup_command, &backup_valid_handler, &profile, &sandbox) == -1) {

    char *header;
    if (asprintf(&header, "A sandboxed app was prevented from opening '%s'", command) == -1) {
        DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
        string_list_free(&forbidden_files);
        string_list_free(&files_to_whitelist);
        free(reason);
        DBGLEAVE(cli?cli->pid:-1, "client_command_process_real");
        return;
    }
    client_notify(cli, "dialog-error", header, "The executed application is not allowed to open at least one of its passed parameters.");
    free(header);

    DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m could not select a sandbox profile to use for command '%s'\n", cli->pid, command);
    client_dialog_run_incompatible(cli, caller_command, command, argv, argc, forbidden_files, files_to_whitelist, backup_command, backup_valid_handler);

    string_list_free(&forbidden_files);
    string_list_free(&files_to_whitelist);
    free(reason);
    DBGLEAVE(cli?cli->pid:-1, "client_command_process_real");
    return;
  }

  free(app_profiles);
  string_list_free(&forbidden_files);
  exechelp_list_free_full(file_profiles, (ExecHelpDestroyNotify) protected_files_handler_free);



  /** third, prepare the execution of the sandbox **/
  // we're now confident that an execution will take place, notify it
  char *header;
  if (asprintf(&header, "%s's system call was delegated to the sandbox", caller_command) == -1) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
      string_list_free(&files_to_whitelist);
      free(reason);
      DBGLEAVE(cli?cli->pid:-1, "client_command_process_real");
      return;
  }
  client_notify(cli, "firejail-protect", header, reason);
  free(header);
  free(reason);

  if (sandbox) {
    client_execute_sandboxed(cli, command, 
                             argv, argc, profile, NULL,
                             is_protected_app,
                             has_protected_files, files_to_whitelist);
  } else {
    client_execute_unsandboxed(cli, command, argv);
  }

  string_list_free(&files_to_whitelist);
  DBGLEAVE(cli?cli->pid:-1, "client_command_process_real");
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

  // get our own identity, useful for displaying notifications
  char *caller_command = pids[cli->pid].cmd;
  if (!caller_command) {
    caller_command = pid_proc_cmdline(cli->pid);
  }
  if (!caller_command) {
    if (asprintf(&caller_command, "Process %d", cli->pid) == -1) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
      DBGLEAVE(cli?cli->pid:-1, "client_command_process");
      return;
    }
  } else
    caller_command = strdup(caller_command);

  // process the command now that we've parsed the message
  command_process_real(cli, caller_command, command, argv, argc);

  // clean up memory
  free(caller_command);
  free(argv);

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
  if (arg_debug)
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

