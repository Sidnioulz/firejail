/* 
    2015 (c) Steve Dodier-Lazaro <sidnioulz@gmail.com>
    This file is part of ExecHelper.

    ExecHelper is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    ExecHelper is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with ExecHelper.  If not, see <http://www.gnu.org/licenses/>.
*/

#define _GNU_SOURCE

#include "../include/common.h"
#include "../include/exechelper.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int firejail_use_notifications = 0;
void exechelp_logv_init(int notifications_available) {
  firejail_use_notifications = notifications_available;
}

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void exechelp_log_force_unlock(void) {
  pthread_mutex_unlock(&log_mutex);
}

static int exechelp_log_make_dir(void) {
  const char *env      = getenv("HOME");
  char       *local    = NULL;
  char       *share    = NULL;
  char       *firejail = NULL;
	struct stat s;

  if (!env)
    return -1;

  if (asprintf(&local, "%s/.local", env) == -1)
    return -1;
	if (stat(local, &s) == -1) {
	  if (mkdir(local, 0755) == -1) {
	    free(local);
	    return -1;
	  }
    int ignore = chown(local, getuid(), getgid());
  }

  if (asprintf(&share, "%s/share", local) == -1) {
    free(local);
    return -1;
  }

  free(local);
	if (stat(share, &s) == -1) {
	  if (mkdir(share, 0755) == -1) {
	    free(share);
	    return -1;
	  }
    int ignore = chown(share, getuid(), getgid());
  }

  if (asprintf(&firejail, "%s/firejail", share) == -1) {
    free(share);
    return -1;
  }

  free(share);
	if (stat(firejail, &s) == -1) {
	  if (mkdir(firejail, 0755) == -1) {
	    free(firejail);
	    return -1;
	  }
    int ignore = chown(firejail, getuid(), getgid());
  }

  return 0;
}

static void exechelp_log_cleanup();

static int exechelp_log_get_handle(const char *id, int reset) {
  static int fd = -1;

  if (fd != -1 && reset) {
    close(fd);
    fd = -1;
    return -1;
  }

  else if (fd == -1 && !reset) {
    exechelp_log_make_dir();

    char        date[100] = {0};
    const char *env       = getenv("HOME");
    char       *path      = NULL;
    time_t      t         = time(NULL);
    struct tm   ttm;

    if (!env)
      return -1;

    localtime_r(&t, &ttm);
    if (!strftime(date, sizeof(date), "%Y-%m-%d_%H%M%S", &ttm))
      date[0] = '\0';

    if (id) {
      if (asprintf(&path, "%s/.local/share/firejail/%s_%d_%s.log", env, id, getpid(), date) == -1)
        return -1;
    } else {
      if (asprintf(&path, "%s/.local/share/firejail/%d_%s.log", env, getpid(), date) == -1)
        return -1;
    }

    fd = open(path, O_CREAT | O_APPEND | O_DSYNC | O_WRONLY, 0644);

    if (fd != -1) {
      int ignore = chown(path, getuid(), getgid());
    }

    free(path);
    atexit(exechelp_log_cleanup);
    return fd;
  }

  return fd;
}

static void exechelp_log_cleanup()
{
  exechelp_log_get_handle(NULL, 1);
}

ssize_t exechelp_log(const char *id, const char *fmt, va_list args) {
  pthread_mutex_lock(&log_mutex);
  ssize_t  ret = 0;
  char    *buf;
  char    *buf2;
  int      fd = exechelp_log_get_handle(id, 0);

  if (fd == -1) {
    pthread_mutex_unlock(&log_mutex);
    return -1;
  }

  if (vasprintf(&buf, fmt, args) == -1) {
    pthread_mutex_unlock(&log_mutex);
    return -1;
  }

  if (id) {
    if (asprintf(&buf2, "[%s] %s", id, buf) == -1) {
      free(buf);
      pthread_mutex_unlock(&log_mutex);
      return -1;
    }

    ret += write(fd, buf2, strlen(buf2));
    free(buf2);
    free(buf);
  }
  else {
    ret += write(fd, buf, strlen(buf));
    free(buf);
  }

  pthread_mutex_unlock(&log_mutex);
  return ret;
}

ssize_t exechelp_logv(const char *id, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  ssize_t ret = exechelp_log(id, fmt, args);
  va_end(args);
  return ret;
}

ssize_t exechelp_logerr(const char *id, int level, const char *fmt, va_list args) {
  vfprintf(stderr, fmt, args);
  if (firejail_use_notifications) {
#ifdef USING_NOTIFICATIONS
//TODO
#endif
  }
  return exechelp_log(id, fmt, args);
}

ssize_t exechelp_logerrv(const char *id, int level, const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  ssize_t ret = exechelp_log(id, fmt, args);
  va_end(args);

  va_start(args, fmt);
  if (firejail_use_notifications && level == FIREJAIL_ERROR) {
#ifdef USING_NOTIFICATIONS
    char *error_text;
    if (asprintf(&error_text, fmt, args) != -1) {

      char *system_text;
      if (asprintf(&system_text, "notify-send -i firejail-error \"Firejail Sandbox Error\" \"%s\"", error_text) != -1) {
        int ignore = system(system_text); // make compilers quiet about ignoring return values
        ignore -= ignore; // make compilers quiet about unused variables
        free(system_text);
      }
      free(error_text);
    }
#endif
  }
  va_end(args);

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);

  return ret;
}

void exechelp_perror(const char *id, const char *msg) {
  exechelp_logv(id, "Error: %s: %s", msg, strerror(errno));
  perror(msg);
}

void exechelp_log_close(void) {
  (void)exechelp_log_get_handle(NULL, 1);
}

