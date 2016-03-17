/*
 * Copyright (C) 2014, 2015 netblue30 (netblue30@yahoo.com)
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
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <errno.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <signal.h>
#include <dirent.h>
#include <string.h>
#include "../include/common.h"

int join_namespace(pid_t pid, char *type) {
	char *path;
	if (asprintf(&path, "/proc/%u/ns/%s", pid, type) == -1)
		errExit("asprintf");
	
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		free(path);
		exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: cannot open /proc/%u/ns/%s.\n", pid, type);
		return -1;
	}

	if (syscall(__NR_setns, fd, 0) < 0) {
		free(path);
		exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: cannot join namespace %s.\n", type);
		close(fd);
		return -1;
	}

	close(fd);
	free(path);
	return 0;
}

// return 1 if error
int name2pid(const char *name, pid_t *pid) {
	pid_t parent = getpid();
	
	DIR *dir;
	if (!(dir = opendir("/proc"))) {
		// sleep 2 seconds and try again
		sleep(2);
		if (!(dir = opendir("/proc"))) {
			exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: cannot open /proc directory\n");
			exit(1);
		}
	}
	
	struct dirent *entry;
	char *end;
	while ((entry = readdir(dir))) {
		pid_t newpid = strtol(entry->d_name, &end, 10);
		if (end == entry->d_name || *end)
			continue;
		if (newpid == parent)
			continue;

		// check if this is a firejail executable
		char *comm = pid_proc_comm(newpid);
		if (comm) {
			// remove \n
			char *ptr = strchr(comm, '\n');
			if (ptr)
				*ptr = '\0';
			if (strcmp(comm, "firejail")) {
				free(comm);
				continue;
			}
			free(comm);
		}
		
		char *cmd = pid_proc_cmdline(newpid);
		if (cmd) {
			// mark the end of the name
			char *ptr = strstr(cmd, "--name=");
			char *start = ptr;
			if (!ptr) {
				free(cmd);
				continue;
			}
			// set a NULL pointer on the next identified argument
			char *next_for_sure = strstr(ptr, " --");
			if (next_for_sure)
		    *next_for_sure = '\0';
			// else we can't allow spaces in the --name argument
			else {
			  while (*ptr != ' ' && *ptr != '\t' && *ptr != '\0')
				  ptr++;
			  *ptr = '\0';
			}
			int rv = strcmp(start + 7, name);
			if (rv == 0) {
				free(cmd);
				*pid = newpid;
				closedir(dir);
				return 0;
			}
			free(cmd);
		}
	}
	closedir(dir);
	return 1;
}

#define BUFLEN 4096
char *pid_proc_comm(const pid_t pid) {
	// open /proc/pid/cmdline file
	char *fname;
	int fd;
	if (asprintf(&fname, "/proc/%d//comm", pid) == -1)
		return NULL;
	if ((fd = open(fname, O_RDONLY)) < 0) {
		free(fname);
		return NULL;
	}
	free(fname);

	// read file
	unsigned char buffer[BUFLEN];
	ssize_t len;
	if ((len = read(fd, buffer, sizeof(buffer) - 1)) <= 0) {
		close(fd);
		return NULL;
	}
	buffer[len] = '\0';
	close(fd);

	// return a malloc copy of the command line
	char *rv = strdup((char *) buffer);
	if (strlen(rv) == 0) {
		free(rv);
		return NULL;
	}
	return rv;
}

char *pid_proc_cmdline(const pid_t pid) {
	// open /proc/pid/cmdline file
	char *fname;
	int fd;
	if (asprintf(&fname, "/proc/%d/cmdline", pid) == -1)
		return NULL;
	if ((fd = open(fname, O_RDONLY)) < 0) {
		free(fname);
		return NULL;
	}
	free(fname);

	// read file
	unsigned char buffer[BUFLEN];
	ssize_t len;
	if ((len = read(fd, buffer, sizeof(buffer) - 1)) <= 0) {
		close(fd);
		return NULL;
	}
	buffer[len] = '\0';
	close(fd);

	// clean data
	int i;
	for (i = 0; i < len; i++) {
		if (buffer[i] == '\0')
			buffer[i] = ' ';
		if (buffer[i] >= 0x80) // execv in progress!!!
			return NULL;
	}

	// return a malloc copy of the command line
	char *rv = strdup((char *) buffer);
	if (strlen(rv) == 0) {
		free(rv);
		return NULL;
	}
	return rv;
}

int string_in_list_comma(const char* list, const char* string) {
  if(!list || !string)
    return 0;

  char *found = strstr(list, string);
  if(!found)
    return 0;

  char next = *(found + strlen(string));
  if(next == '\0' || next == ',')
    return 1;
  else
    return 0;
}

int string_in_list(const char* list, const char* string) {
  if(!list || !string)
    return 0;

  char *found = strstr(list, string);
  if(!found)
    return 0;

  char next = *(found + strlen(string));
  if(next == '\0' || next == ':')
    return 1;
  else
    return 0;
}

char *string_list_flatten(char **list, char *sep) {
  char   *result  = NULL;
  size_t  index   = 0;
  size_t  len     = 0;
  size_t  printed = 0;
  size_t  seplen  = 0;

  if (!list)
    return NULL;

  if (list[0] == NULL)
    return strdup("");

  seplen = sep ? strlen(sep) : 0;
  for(index = 0; list[index]; index++) {
    len += strlen(list[index]) + seplen;
  }

  result = malloc((++len) * sizeof(char)); // adding 1 for ending "\0"
  if (!result)
    return NULL;

  printed = snprintf(result, len, "%s", list[0]);
  for(index = 1; list[index]; index++) {
    printed += snprintf (result+printed, len, "%s%s", sep, list[index]);
  }

  return result;
}

int string_list_is_empty(char **list) {
  if (list == NULL)
    return 1;

  return list[0] == NULL;
}

void string_list_append(char ***list, char *item) {
  if (!list)
    return;

  if (!*list) {
    *list = malloc(sizeof(char *) * 2);
    (*list)[0] = item;
    (*list)[1] = NULL;
    return;
  }

  size_t i;
  for(i = 0; (*list)[i]; i++);

  *list = realloc(*list, sizeof(char *) * (i + 2));
  (*list)[i] = item;
  (*list)[i+1] = NULL;
}

int strcmp_comma(const char* s1, const char* s2) {
  while(*s1 && (*s1==*s2) && *s1!=',')
    s1++,s2++;

  // comma and null character are equivalent here
  if ((*s1 == ',' || *s1 == '\0') && (*s2 == ',' || *s2 == '\0'))
    return 0;

  return *(const unsigned char*)s1-*(const unsigned char*)s2;
}

char **string_list_copy(char **other) {
  char   **self = NULL;
  size_t   len  = 0;
  if (!other)
    return NULL;

  for(len = 0; other[len]; len++);
  self = malloc(sizeof(char *) * (len + 1));

  for(len = 0; other[len]; len++)
    self[len] = other[len] ? strdup(other[len]) : NULL;
  self[len] = NULL;

  return self;
}

void string_list_free(char ***list) {
  printf("list_freeing %p\n", *list);
  
  if (!list)
    return;

  if (!*list)
    return;

  for (size_t i=0; (*list)[i]; i++)
    free((*list)[i]);

  free(*list);
  *list = NULL;
}

char *split_comma(char *str) {
	if (str == NULL || *str == '\0')
		return NULL;
	char *ptr = strchr(str, ',');
	if (!ptr)
		return NULL;
	*ptr = '\0';
	ptr++;
	if (*ptr == '\0')
		return NULL;
	return ptr;
}

int firejail_strcmp(const char *a, const char *b) {
  if (!a)
    return b? -1 : 0;
  else if (!b)
    return 1;
  else
    return strcmp(a, b);
}
