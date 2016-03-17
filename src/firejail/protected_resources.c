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
#include "../include/exechelper.h"
#include <errno.h>

static int command_is_protected = -1; // whether the command to be run by firejail is protected by the global policy

static int profile_name_matches_current(const char *name) {
  if (!cfg.profile_name)
    return 0;
  else
    return strcmp(name, cfg.profile_name) == 0;
}

static int command_name_matches_current(const char *name) {
  if (!cfg.command_name)
    return 0;
  else {
    char *realname = exechelp_resolve_executable_path(name);
    char *realcommand = exechelp_resolve_executable_path(cfg.command_name);

    int identical = realname && realcommand && strcmp(realname, realcommand) == 0;

    return identical;
  }
}

int is_current_command_protected(void) {
  if(command_is_protected == -1) {
    // will update command_is_protected
    char *whatever = get_protected_apps_for_client();
    free(whatever);
  }

  return command_is_protected;
}

char *get_protected_apps_for_client (void) {
  char *result = NULL;

  if (arg_debug)
		printf("Looking up user's config directory for a list of protected apps\n");
  char *filepath;
	if (asprintf(&filepath, "%s/.config/firejail/%s", cfg.homedir, EXECHELP_PROTECTED_APPS_BIN) == -1)
		errExit("asprintf");
	FILE *fp = fopen(filepath, "rb");
	free(filepath);

  if (fp == NULL) {
    if (arg_debug)
		  printf("Looking up /etc/firejail for a list of protected apps\n");
    char *filepath;
	  if (asprintf(&filepath, "/etc/firejail/%s", EXECHELP_PROTECTED_APPS_BIN) == -1)
		  errExit("asprintf");
  	fp = fopen(filepath, "rb");
  	free(filepath);
  }

	if (fp == NULL) {
		exechelp_logerrv("firejail", FIREJAIL_WARNING, "Warning: could not find any file listing protected apps\n");
	  return NULL;
	}

  // assuming the current command isn't protected, but we'll flag it if we see it
  char *current_binary = exechelp_resolve_executable_path(cfg.command_name);
  command_is_protected = 0;

  if (arg_debug)
  	printf("Reading the list of protected apps from a file\n");

	// read the file line by line
	char buf[EXECHELP_POLICY_LINE_MAX_READ + 1];
	int lineno = 0, command_seen, command_allowed;
	while (fgets(buf, EXECHELP_POLICY_LINE_MAX_READ, fp)) {
		++lineno;
		if (*buf == '#') // comment, ignore this line
		  continue;
		command_seen = 0, command_allowed = 0;

    // check for overflows
    if (strlen(buf) == EXECHELP_POLICY_LINE_MAX_READ) {
      exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: line %d of protected-apps.policy is too long, it should be no longer than %d characters\n", lineno, EXECHELP_POLICY_LINE_MAX_READ);
	    return NULL;
    }

    // remove the trailing "\n"
    buf[strlen(buf)-1] = '\0';

    // locate the separator, and replace it with a null character
    char *sep = NULL;
    if (!exechelp_parse_get_next_separator(buf, &sep, 1)) {
      if (arg_debug)
        exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: line %d is malformed, should be of the form: <path to file>///<profile name>:<path to handler>,<profile name>:<path to handler>,...\n", lineno);
	    return NULL;
    }

    // does the current line match our own binary?
    if (current_binary && strcmp(current_binary, buf) == 0) {
      command_seen = 1;
    }

    // if that binary is linked to the current profile, we don't care
    if (is_command_linked_for_client(buf)) {
      command_allowed = 1;
    }

    // now check the profile
    int found = 0;
    char *ptr = sep, *prev;
    while (ptr != NULL) {
      prev = ptr;
      ptr = split_comma(ptr);
      if (profile_name_matches_current(prev)) {
        found = 1;
        break;
      }
    }

    // the command is protected but matches our execution context, remember seeing it and don't put it on the forbidden list
    if (command_seen && found) {
      command_is_protected = 1;
      command_allowed = 1;
    }

    // add the current line's binary to our list of protected binaries  
    if (!command_allowed) {
      int written = result ? asprintf(&result, "%s:%s", result, buf)
                   : asprintf(&result, "%s", buf);
      if (written == -1)
        errExit("asprintf");
    }
	}

  if (arg_debug)
    printf("The following apps are protected from being invoked by child process: %s\n", result); 
	fclose(fp);
	free(current_binary);

  return result;
}

char *get_protected_files_for_client (int blacklist) {
  char *result = NULL;

  if (arg_debug)
		printf("Looking up user's config directory for a list of protected files\n");
  char *filepath;
	if (asprintf(&filepath, "%s/.config/firejail/%s", cfg.homedir, EXECHELP_PROTECTED_FILES_BIN) == -1)
		errExit("asprintf");
	FILE *fp = fopen(filepath, "rb");
	free(filepath);

  if (fp == NULL) {
    if (arg_debug)
		  printf("Looking up /etc/firejail for a list of protected files\n");
    char *filepath;
	  if (asprintf(&filepath, "/etc/firejail/%s", EXECHELP_PROTECTED_FILES_BIN) == -1)
		  errExit("asprintf");
  	fp = fopen(filepath, "rb");
  	free(filepath);
  }

	if (fp == NULL) {
		exechelp_logerrv("firejail", FIREJAIL_WARNING, "Warning: could not find any file listing protected files\n");
	  return NULL;
	}

  if (arg_debug)
  	printf("Reading the list of protected files from a file\n");

	// read the file line by line
	char buf[EXECHELP_POLICY_LINE_MAX_READ + 1];
	int lineno = 0;
	while (fgets(buf, EXECHELP_POLICY_LINE_MAX_READ, fp)) {
		++lineno;
		if (*buf == '#') // comment, ignore this line
		  continue;

    // check for overflows
    if (strlen(buf) == EXECHELP_POLICY_LINE_MAX_READ) {
      exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: line %d of protected-files.policy is too long, it should be no longer than %d characters\n", lineno, EXECHELP_POLICY_LINE_MAX_READ);
	    return NULL;
    }

    // remove the trailing "\n"
    buf[strlen(buf)-1] = '\0';

    // locate the separator, and replace it with a null character
    char *sep = NULL;
    if (!exechelp_parse_get_next_separator(buf, &sep, 1)) {
      if (arg_debug)
        exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: line %d is malformed, should be of the form: <path to file>///<profile name>:<path to handler>,<profile name>:<path to handler>,...\n", lineno);
	    return NULL;
    }

    /* note that at this stage, buf only contains the file path because
     * parse_get_next_separator injects a '\0' instead of the separator
     */

    char *realpath = exechelp_coreutils_realpath(buf);
    if (!realpath) {
      if (arg_debug)
        exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: line %d is probably malformed, the first part of the line cannot be converted into a valid UNIX path\n", lineno);
      return NULL;
    }


    // split the remaining string starting from past the separator
    int found = 0;
    char *ptr = sep, *prev;
    while (ptr != NULL) {
      prev = ptr;
      ptr = split_comma(ptr);
      
      // split ptr into profile and path
      char *path = strchr(prev, ':');

      if (!path) {
        exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: line %d is malformed, should be of the form: <file>\\0<profile name>:<path to handler>,<profile name>:<path to handler>\n", lineno);
        return NULL;
      }

      *path = '\0';
      path++;

      if ((profile_name_matches_current(prev) && (strcmp("*", path) == 0 || command_name_matches_current(path)))
          || (strcmp("*", prev) == 0 && command_name_matches_current(path))) {
        found = 1;
        break;
      }
    }

    /* at this point we know if the profile matches the app to be run, and we know the file path */

    /* if the profile matches, we want to add the file to the whitelist, but keep it here so firejail understands this instance runs protected files */
    if (found) {
      if (arg_whitelist_files) {
        char *tmp;
        if (asprintf(&tmp, "%s,%s", arg_whitelist_files, realpath) == -1)
          errExit("asprintf");
        else {
          free(arg_whitelist_files);
          arg_whitelist_files = tmp;
        }
      } else {
        arg_whitelist_files = strdup(realpath);
      }
    }
    /* if the file is not whitelisted, then it's blacklisted */
    else if (blacklist && !exechelp_file_list_contains_path(arg_whitelist_files, realpath, NULL)) {
      fs_blacklist_file(realpath);
    }

    int written = result ? asprintf(&result, "%s:%s", result, realpath)
                         : asprintf(&result, "%s", realpath);
    if (written == -1)
      errExit("asprintf");

    free(realpath);
	}

  if (arg_debug)
    printf("The following files are protected from being opened by child process: %s\n", result); 
	fclose(fp);

  return result;
}

