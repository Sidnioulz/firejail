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

#ifndef __EH_EXECHELP_LOGGER_H__
#define __EH_EXECHELP_LOGGER_H__

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

ssize_t exechelp_log(const char *id, const char *fmt, va_list args);
ssize_t exechelp_logv(const char *id, const char *fmt, ...);
ssize_t exechelp_logerr(const char *id, const char *fmt, va_list args);
ssize_t exechelp_logerrv(const char *id, const char *fmt, ...);
void exechelp_perror(const char *id, const char *str);
void exechelp_log_close(void);

extern int arg_debug;
struct timeval dbg_tv;
time_t         dbg_t;
struct tm      dbg_tm;
char           dbg_line[200];

#define DBGOUT(fmt, ...) \
 do { \
  (void)gettimeofday(&dbg_tv,NULL); \
  (void)time(&dbg_t); \
  (void)localtime_r(&dbg_t, &dbg_tm); \
  fprintf(stdout, "%2.2d:%2.2d:%2.2d ", dbg_tm.tm_hour, dbg_tm.tm_min, dbg_tm.tm_sec); \
  fprintf(stdout, fmt, ##__VA_ARGS__); \
  exechelp_logv("fireexecd", "%2.2d:%2.2d:%2.2d ", dbg_tm.tm_hour, dbg_tm.tm_min, dbg_tm.tm_sec); \
  exechelp_logv(NULL, fmt, ##__VA_ARGS__); \
  fflush(stdout); \
} while (0)

#define DBGERR(fmt, ...) \
 do { \
  (void)gettimeofday(&dbg_tv,NULL); \
  (void)time(&dbg_t); \
  (void)localtime_r(&dbg_t, &dbg_tm); \
  fprintf(stderr, "%2.2d:%2.2d:%2.2d ", dbg_tm.tm_hour, dbg_tm.tm_min, dbg_tm.tm_sec); \
  fprintf(stderr, fmt, ##__VA_ARGS__); \
  exechelp_logv("fireexecd", "%2.2d:%2.2d:%2.2d ", dbg_tm.tm_hour, dbg_tm.tm_min, dbg_tm.tm_sec); \
  exechelp_logv(NULL, fmt, ##__VA_ARGS__); \
  fflush(stderr); \
} while (0)

#define DBGENTER(id, name) \
 do { \
  if(arg_debug >= 2) { \
    (void)gettimeofday(&dbg_tv,NULL); \
    (void)time(&dbg_t); \
    (void)localtime_r(&dbg_t, &dbg_tm); \
    fprintf(stdout, "\e[0;30m%2.2d:%2.2d:%2.2d ", dbg_tm.tm_hour, dbg_tm.tm_min, dbg_tm.tm_sec); \
    fprintf(stdout, "\e[0;30m[%d]\t(entering %s)\e[0;0m\n", id, name); \
    exechelp_logv("fireexecd", "\e[0;30m%2.2d:%2.2d:%2.2d ", dbg_tm.tm_hour, dbg_tm.tm_min, dbg_tm.tm_sec); \
    exechelp_logv(NULL, "\e[0;30m[%d]\t(entering %s)\e[0;0m\n", id, name); \
    fflush(stdout); \
  }\
} while (0)

#define DBGLEAVE(id, name) \
 do { \
  if(arg_debug >= 2) { \
    (void)gettimeofday(&dbg_tv,NULL); \
    (void)time(&dbg_t); \
    (void)localtime_r(&dbg_t, &dbg_tm); \
    fprintf(stdout, "\e[0;30m%2.2d:%2.2d:%2.2d ", dbg_tm.tm_hour, dbg_tm.tm_min, dbg_tm.tm_sec); \
    fprintf(stdout, "\e[0;30m[%d]\t(leaving %s)\e[0;0m\n", id, name); \
    exechelp_logv("fireexecd", "\e[0;30m%2.2d:%2.2d:%2.2d ", dbg_tm.tm_hour, dbg_tm.tm_min, dbg_tm.tm_sec); \
    exechelp_logv(NULL, "\e[0;30m[%d]\t(leaving %s)\e[0;0m\n", id, name); \
    fflush(stdout); \
  }\
} while (0)

#endif /* __EH_EXECHELP_LOGGER_H__ */
