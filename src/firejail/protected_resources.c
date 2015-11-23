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
    char *realname = exechelp_resolve_path(name);
    char *realcommand = exechelp_resolve_path(cfg.command_name);

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
	FILE *fp = fopen(EXECHELP_PROTECTED_APPS_BIN, "rb");
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
		fprintf(stderr, "Warning: could not find any file listing protected apps\n");
	  return NULL;
	}

  // assuming the current command isn't protected, but we'll flag it if we see it
  char *realcommand = exechelp_resolve_path(cfg.command_name);
  command_is_protected = 0;

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

    // if that process is linked to the current profile, we don't care
    if (is_command_linked_for_client(process)) {
      if(realcommand) {
        // we don't actually mark the command as protected (we're about to run it!) but we remember seeing it
        if(strcmp(realcommand, process) == 0)
          command_is_protected = 1;
      }
      continue;
    }

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
    
    // split the string
    int found = 0;
    char *ptr = proflist, *prev;
    while (ptr != NULL) {
      prev = ptr;
      ptr = split_comma(ptr);
      if (profile_name_matches_current(prev)) {
        found = 1;
        break;
      }
    }

    if (found)
      continue;
  
    int written;
    if (result)
      written = asprintf(&result, "%s:%s", result, process);
    else
      written = asprintf(&result, "%s", process);
    if (written == -1)
      errExit("asprintf");
	}

  if (arg_debug)
    printf("The following apps are protected from being invoked by child process: %s\n", result); 
	fclose(fp);
	free(realcommand);

  return result;
}

char *get_protected_files_for_client (void) {
  char *result = NULL;

  if (arg_debug)
		printf("Looking up user's config directory for a list of protected files\n");
  char *filepath;
	if (asprintf(&filepath, "%s/.config/firejail/%s", cfg.homedir, EXECHELP_PROTECTED_FILES_BIN) == -1)
		errExit("asprintf");
	FILE *fp = fopen(EXECHELP_PROTECTED_FILES_BIN, "rb");
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
		fprintf(stderr, "Warning: could not find any file listing protected files\n");
	  return NULL;
	}

  if (arg_debug)
  	printf("Reading the list of protected files from a file\n");

	// read the file line by line
	char buf[EXECHELP_POLICY_LINE_MAX_READ + 1], file[EXECHELP_POLICY_LINE_MAX_READ + 1], proflist[EXECHELP_POLICY_LINE_MAX_READ + 1];
	int lineno = 0;
	while (fgets(buf, EXECHELP_POLICY_LINE_MAX_READ, fp)) {
		++lineno;

    // get current line's file
    int pathlen = snprintf(file, EXECHELP_POLICY_LINE_MAX_READ, "%s", buf);
    if (pathlen == -1)
      errExit("snprintf");
    else if (pathlen >= EXECHELP_POLICY_LINE_MAX_READ) {
      fprintf(stderr, "Error: line %d of protected-file.bin in malformed, the file path is longer than %d characters and the profile list cannot be read\n", lineno, EXECHELP_POLICY_LINE_MAX_READ);
      errno = ENAMETOOLONG;
      errExit("snprintf");
    }

    int proflen = snprintf(proflist, EXECHELP_POLICY_LINE_MAX_READ - pathlen - 1, "%s", buf + pathlen + 1);
    if (proflen == -1)
      errExit("snprintf");
    else if (*(buf + pathlen + proflen) != '\n') {
      fprintf(stderr, "Error: line %d of protected-file.bin in malformed, the line is longer than %d characters and the profile list cannot be read\n", lineno, EXECHELP_POLICY_LINE_MAX_READ);
      errno = ENAMETOOLONG;
      errExit("snprintf");
    }

    // remove the trailing \n before splitting the string
    proflist[proflen-1] = '\0';
    
    // split the string
    int found = 0;
    char *ptr = proflist, *prev;
    while (ptr != NULL) {
      prev = ptr;
      ptr = split_comma(ptr);
      
      // split ptr into profile and path
      char *path = strchr(prev, ':');

      if (!path) {
        fprintf(stderr, "Error: line %d in malformed, should be of the form: <file>\\0<profile name>:<path to handler>,<profile name>:<path to handler>\n", lineno);
        errExit("strchr");
      }

      *path = '\0';
      path++;

      if ((profile_name_matches_current(prev) && (strcmp("*", path) == 0 || command_name_matches_current(path)))
          || (strcmp("*", prev) == 0 && command_name_matches_current(path))) {
        found = 1;
        break;
      }
    }

    if (found)
      continue;
  
    int written;
    if (result)
      written = asprintf(&result, "%s:%s", result, file);
    else
      written = asprintf(&result, "%s", file);
    if (written == -1)
      errExit("asprintf");
	}

  if (arg_debug)
    printf("The following files are protected from being opened by child process: %s\n", result); 
	fclose(fp);

  return result;
}

