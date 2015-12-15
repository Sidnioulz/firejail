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

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "../include/common.h"
#include "../include/exechelper.h"

int arg_debug = DEBUGLVL;
pthread_mutex_t commandmutex = PTHREAD_MUTEX_INITIALIZER;


void __attribute__ ((constructor)) load(void);

// Called when the library is loaded and before dlopen() returns
void load(void) {
  char *debug = getenv(EXECHELP_DEBUG_ENV);
  if (!debug)
    arg_debug = 0;
  else {
    errno = 0;
    arg_debug = strtol(debug, NULL, 10);
    if (errno) {
      fprintf(stderr, "Warning: environment variable FIREJAIL_DEBUG should be a number. Increasing the number will increase the level of debug.\n");
      arg_debug = 0;
    }
  }

  if (arg_debug >= 2)
    printf("DEBUG: starting Firejail's execution helper in debug mode.\n");
}


#define _SERVER_SOCKET_COUNTER_LIMIT_SECS   10
#define _SERVER_SOCKET_COUNTER_LIMIT_200MS  50
static int exechelp_get_command_socket(int closing) {
  static int fd = -1;

  if(fd >= 0 && closing) {
    close(fd);
    if(arg_debug)
      fprintf(stdout, "Child process closed its connection to the sandbox helper.\n");
    fd = -1;
  }

  if (fd < 0 && !closing) {
    char *path = getenv(EXECHELP_COMMANDS_SOCKET);

    if(!path) {
      fprintf(stderr, "Error: Child process was not given the path of the socket it should use to communicate with the sandbox helper (missing environment variable %s)\n", EXECHELP_COMMANDS_SOCKET);
      fd = -1;
      return fd;
    }

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
      fprintf(stderr, "Error: Child process could not create a UNIX socket (error: %s)\n", strerror(errno));
      fd = -1;
      return fd;
    }

    struct sockaddr_un remote;
    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, path);
    ssize_t len = strlen(remote.sun_path) + sizeof(remote.sun_family);
          
    // attempt to connect for the next 10 seconds
    struct timespec rqtp = {0, 200000000};
    int counter = 0, connected = 0;

    while(!connected && counter < _SERVER_SOCKET_COUNTER_LIMIT_200MS) {
      if (connect(fd, (struct sockaddr *)&remote, len) == -1) {
        // still waiting on fireexecd
        if (errno == ENOENT) {
          fprintf(stderr, "Warning: Child process could not connect to the sandbox helper, as the helper daemon is not ready yet. Retrying...\n");
          nanosleep(&rqtp, NULL);
          counter++;
        } else {
          fprintf(stderr, "Error: Child process could not connect to the sandbox helper (error: %s)\n", strerror(errno));
          close(fd);
          fd = -1;
          return fd;
        }
      } else {
        connected = 1;
      }
    }

    // 10 seconds elapsed without managing to connect... fireexecd is probably not running or has crashed
    if(counter == _SERVER_SOCKET_COUNTER_LIMIT_200MS) {
      fprintf(stderr, "Warning: Child process could not connect to the sandbox helper, are you sure the helper daemon is running?\n");
      close(fd);
      fd = -1;
      return fd;
    }
  }

  return fd;
}

int exechelp_send_command(const char const  *command, char *argv[])
{
  pthread_mutex_lock(&commandmutex);

  int fd = exechelp_get_command_socket(0);
  if (fd == -1) {
    fprintf(stderr, "Error: could not get a socket to send the command to the sandbox helper\n");
    goto send_command_failed;
  }

  char msg[EXECHELP_COMMAND_MAXLEN+1], *ptr = NULL;
  msg[EXECHELP_COMMAND_MAXLEN] = '\0';
  int written, lenleft = EXECHELP_COMMAND_MAXLEN;
  if((written = snprintf(msg, EXECHELP_COMMAND_MAXLEN, "%s", command)) == -1) {
    fprintf(stderr, "Error: snprintf() call failed in exechelp_send_command (error: %s)\n", strerror(errno));
    goto send_command_failed;
  }
  ptr = msg + (written + 1);
  lenleft = lenleft - (written + 1);
  //printf("ptr: %p\twrote: %d/%d\tleft: %d\tnow: %s\n", ptr, EXECHELP_COMMAND_MAXLEN - lenleft, written+1, lenleft, command);

  int index = argv? (argv[0]? 1:0) : 0;
  for (; argv && argv[index] != NULL; index++) {
    //printf("ptr: %p\twrote: %d/%d\tleft: %d\tnow: %s\n", ptr, EXECHELP_COMMAND_MAXLEN - lenleft, written+1, lenleft, argv[index]);
    if((written = snprintf(ptr, lenleft, "%s",  argv[index])) == -1) {
      fprintf(stderr, "Error: snprintf() call failed in exechelp_send_command (error: %s)\n", strerror(errno));
      goto send_command_failed;
    }

    ptr = ptr + (written + 1);
    lenleft = lenleft - (written + 1);
  }

  if (lenleft == 0) {
    fprintf(stderr, "Critical: child process attempted to send a command that is too long for the sandbox helper (exceeds %d bytes)\n", EXECHELP_COMMAND_MAXLEN);
    goto send_command_failed;
  }

  if (send(fd, msg, EXECHELP_COMMAND_MAXLEN - lenleft, 0) == -1) {
    fprintf(stderr, "Error: could not send the command to the sandbox helper (error: %s)\n", strerror(errno));
    goto send_command_failed;
  } else {
    if (arg_debug)
      fprintf(stdout, "Child process sent a message to the sandbox helper (length %d) to execute '%s'\n", EXECHELP_COMMAND_MAXLEN - lenleft, command);
  }

  exechelp_get_command_socket(1);
  pthread_mutex_unlock(&commandmutex);
  return 0;

send_command_failed:
  exechelp_get_command_socket(1);
  pthread_mutex_unlock(&commandmutex);
  return -1;
}

static int exechelp_is_whitelisted_app(const char *target)
{
  if (arg_debug)
    printf("Child process determining whether '%s' is white-listed for this firejail instance...", target);
  int found = 0;

  if (!target)
    goto ret;

  char *associations = exechelp_read_list_from_file(EXECHELP_WHITELIST_APPS_PATH);
  if (!associations) {
    if (arg_debug >= 2)
      printf("DEBUG: Could not find a list of white-listed apps against which to check '%s'\n", target);
    goto ret;
  }

  found = string_in_list(associations, target);

  // white-list self... we didn't do it before to let firejail compute whether this instance semantically
  // is a protected or untrusted one (that depends on the profile given to us at launch time).
  if (!found) {
    char *path = exechelp_resolve_executable_path(program_invocation_name);
    if (path) {
      if (strcmp(path, target) == 0) {
        if (arg_debug >= 2)
          printf("DEBUG: Child process's own binary is white-listed\n"); 
        found = 1;
      }
      free(path);
    }
  }

  ret:
  if (arg_debug)
    printf(" %s\n", (found? "Yes" : "No"));
  return found;
}

/**
 * @fn exechelp_is_linked_app
 * @brief Tells whether a specific binary is linked with the currently
 * running program by its sandbox profile
 *
 * @param target: the full path of the binary to be queried
 * @return 1 if target is linked with the running app, 0 otherwise
 */
static int exechelp_is_linked_app(const char *target)
{
  if (arg_debug)
    printf("Child process determining whether '%s' is an authorised linked application...", target);
  int found = 0;

  if (!target)
    goto ret;

  char *associations = exechelp_read_list_from_file(EXECHELP_LINKED_APPS_PATH);
  if (!associations) {
    if (arg_debug >= 2)
      printf("DEBUG: Could not find a list of linked apps against which to check '%s'\n", target);
    goto ret;
  }

  found = string_in_list(associations, target);

  ret:
  if (arg_debug)
    printf(" %s\n", (found? "Yes" : "No"));
  return found;
}

/**
 * @fn exechelp_is_sandbox_protected_app
 * @brief Tells whether a specific binary should be executed in a special
 * environment by the sandbox helper rather than in the current process
 *
 * @param target: the full path of the binary to be queried
 * @return 1 if target is to be managed by the sandbox, 0 otherwise
 */
static int exechelp_is_sandbox_protected_app(const char *target)
{
  if (arg_debug)
    printf("Child process determining whether '%s' is a forbidden program within the sandbox...", target);
  int found = 0;

  if (!target)
    goto ret;

  char *managed = exechelp_read_list_from_file(EXECHELP_PROTECTED_APPS_PATH);
  if (!managed) {
    if (arg_debug >= 2)
      printf("DEBUG: Could not find a list of protected apps against which to check '%s'\n", target);
    goto ret;
  }

  found = string_in_list(managed, target);

  ret:
  if (arg_debug)
    printf(" %s\n", (found? "Yes" : "No"));
  return found;
}

static int _exechelp_is_whitelisted_file(const char *real, int dbg)
{
  if (dbg)
    printf("Child process determining whether '%s' is a white-listed file for this firejail instance...", real);

  if (!real)
    goto ret;

  char *associations = exechelp_read_list_from_file(EXECHELP_WHITELIST_FILES_PATH);
  if (!associations) {
    if (dbg >= 2)
      printf("DEBUG: Could not find a list of white-listed files against which to check '%s'\n", real);
    goto ret;
  }

  const char *iter = associations;
  int found = 0;

  while(iter && iter[0]!='\0' && !found) {
    found = exechelp_str_has_prefix_on_sep(real, iter, ':');
    iter = strstr(iter, ":");
    if (iter)
      iter++;
  }

  ret:
  if (dbg)
    printf(" %s\n", (found? "Yes" : "No"));
  return found;
}

/*static int exechelp_is_whitelisted_file(const char *real)
{
  return _exechelp_is_whitelisted_file (real, arg_debug);
}*/

int exechelp_targets_any_sandbox_protected_file(const char *command, char *const argv[]) {
  if (arg_debug)
    printf("Child process determining whether arguments passed to execve('%s') contain forbidden files...", command);
  if (arg_debug >= 2)
    printf("%s", "\n");

  // get a list of protected files first
  char *managed = exechelp_read_list_from_file(EXECHELP_PROTECTED_FILES_PATH);
  if (!managed) {
    if (arg_debug >= 2)
      printf("DEBUG: Could not find a list of sandbox-managed files to check arguments before executing '%s'\n", command);
    return 0;
  }

  // prepare the return structure
  int len = 0, some_forbidden = 0;
  for(;argv[len];++len);

  // start looping
  if (arg_debug >= 2)
    printf("DEBUG: %d arguments will be examined\n", len);
    
  for (len=1; argv[len] && !some_forbidden; ++len) {
    if (arg_debug >= 2)
      printf("DEBUG: checking if argument %d ('%s') is to be managed by the sandbox\n", len, argv[len]);

    // get the canonical path of the current argument, if any
    char *real = exechelp_coreutils_realpath(argv[len]);

    // try to figure out if the parameter is a file
    short is_file = strchr(argv[len], '/') != NULL;
    if (!is_file) {
      struct stat sb;
      if (stat(real, &sb) == 0)
        is_file = 1;
      else
        is_file = (errno == EACCES || errno == ELOOP || errno == EOVERFLOW) ? 1:0;
    }

    // debugging
    if (is_file && arg_debug >= 2)
      printf("DEBUG: \t\t'%s' is believed to be a file, located at '%s'\n", argv[len], real);
    else if (arg_debug >= 2)
      printf("DEBUG: \t\t'%s' is believed not to be a file\n", argv[len]);

    // check if the current argument is white-listed
    char *prefix = NULL;
    if (_exechelp_is_whitelisted_file(real, 0)) {
      if (arg_debug >= 2)
        printf("DEBUG: \t\t'%s' is white-listed for this firejail instance\n", argv[len]);
    }
    // check if the current argument is a protected file
    else if (exechelp_file_list_contains_path(managed, real, &prefix)) {
      if (arg_debug >= 2)
        printf("DEBUG: \t\t'%s' is forbidden within the sandbox (matches '%s')\n", argv[len], prefix);
      
      // file is protected, indicate we found something forbidden
      some_forbidden = 1;
      free(prefix);
    }
    // the current argument is not white-listed or protected
    else {
      if (arg_debug >= 2)
        printf("DEBUG: \t\t'%s' is allowed within the sandbox\n", argv[len]);
    }
  }

  if (arg_debug >= 2)
    printf("%s", "Found forbidden files in arguments?");
  if (arg_debug)
    printf(" %s\n", some_forbidden? "Yes" : "No");
  return some_forbidden;
}

static int exechelp_filter_forbidden_exec(const char *target, char *const argv[], char *const envp[],
                                          char **allowed_target, char **allowed_argv[],
                                          char **forbidden_target, char **forbidden_argv[])
{
  if(!target || !argv)
    return 0;
  size_t arg_len = 0;
  while(argv[arg_len++]);

  char *polenv = getenv(EXECHELP_EXECUTION_POLICY);
  int pol = -1;
  if (polenv) {
    errno = 0;
    pol = strtol(polenv, NULL, 10);
    if (errno) {
      fprintf(stderr, "Error: could not read environment variable EXECHELP_EXECUTION_POLICY, was not a number as expected (do not attempt to manually set the policy, this is done automatically by firejail)\n");
      pol = -1;
    }
    if (arg_debug) {
      printf("The security policy to be followed by child process is set in the environment as %s\n", polenv);
    }
  }

  if (pol == -1) {
    if (arg_debug) {
      printf("Using the default program execution policy %d\n", EXECHELP_DEFAULT_POLICY);
    }  
    pol = EXECHELP_DEFAULT_POLICY;
  }

  int whitelisted = exechelp_is_whitelisted_app(target);
  int linked = exechelp_is_linked_app(target);
  int protected = exechelp_is_sandbox_protected_app(target);

  if (whitelisted)
    goto binary_clear;
  else if (linked && (pol & LINKED_APP))
    goto binary_clear;
  else if (!linked && protected && pol & SANDBOX_PROTECTED)
    goto binary_clear;
  else if (!linked && !protected && pol & UNSPECIFIED)
    goto binary_clear;
  else
    goto binary_forbidden;

  binary_clear:
  {
    if (arg_debug >= 2)
      printf("DEBUG: Child process can partly or completely execute '%s', now checking parameters...\n", target);

    /* Don't do anything fancy yet for mixed forbidden-allowed executions, just
     * delegate to the sandbox but let it know to expect a mixed setup, we can
     * then ask users how they want the execution handled
     */
    if (exechelp_targets_any_sandbox_protected_file(target, argv))
      goto binary_forbidden;

    if (arg_debug >= 2)
      printf("DEBUG: Child process is allowed to execute '%s' and to access all of its parameters, proceeding\n", target);
    *allowed_target = strdup(target);
    *allowed_argv = arg_len? exechelp_malloc0(sizeof(char *) * (arg_len+1)) : NULL;
    *allowed_argv = memcpy(*allowed_argv, argv, sizeof(char *) * (arg_len));

    return 1;
  }

  binary_forbidden:
  {
    if (arg_debug >= 2)
      printf("DEBUG: Child process is not allowed to execute '%s', or some parameters are are not allowed; delegating the whole execution\n", target);
    *forbidden_target = strdup(target);
    *forbidden_argv = arg_len? exechelp_malloc0(sizeof(char *) * (arg_len+1)) : NULL;
    *forbidden_argv = memcpy(*forbidden_argv, argv, sizeof(char *) * (arg_len));

    return 0;
  }
}

/* int execl(const char *path, const char *arg, ...) will call execve */
/* int execle(const char *path, const char *arg, ...) will call execve */
/* int execlp(const char *file, const char *arg, ...) will call execvp */
/* int execv(const char *path, char *const argv[]) will call execve */
/* int execvp(const char *file, char *const argv[]) will call execvpe */

int execve(const char *path, char *const argv[], char *const envp[])
{
  typeof(execve) *original_execve = dlsym(RTLD_NEXT, "execve");
  if(!original_execve) {
    fprintf(stderr, "Critical: the child process is not properly loading the standard C library, and libexechelper cannot find the real implementation of execve(), aborting\n");
    return EACCES;
  }
  if (arg_debug)
    printf("Child process is attempting to execute (execve) binary '%s'\n", path);

  if (!path) {
    errno = ENOENT;
    return -1;
  }

  if (access(path, R_OK | X_OK)) {
    if (arg_debug)
      printf("Child process cannot execute '%s' as this binary cannot be accessed, aborting\n", path);
    // we keep the errno from access
    return -1;
  }

  char *allowed_exec = NULL, *forbidden_exec = NULL;
  char **allowed_argv = NULL, **forbidden_argv = NULL;
  int ret_value = 0;

  exechelp_filter_forbidden_exec(path, argv, envp,
                                 &allowed_exec, &allowed_argv,
                                 &forbidden_exec, &forbidden_argv);

  /* First getting rid of the denied process/files because we know we will return from this call. */
  if (forbidden_exec) {
    if (!allowed_exec && arg_debug)
      printf("Child process delegating the execution of '%s' to the sandbox\n", path); 
    else if (arg_debug)
      printf("Child process must delegate the execution of some parameters of '%s' to the sandbox\n", path);

    if (exechelp_send_command(forbidden_exec, forbidden_argv) != -1) {
      if (arg_debug)
        printf("Child process's execve() system call successfully hijacked for sandbox to take over\n");
    } else if (arg_debug)
      printf("Error: Child process's system call should have been sent to the sandbox helper for execution, but an error occurred\n");
  }

  /* Then executing the allowed process with the allowed parameters. We might not return. */
  if (allowed_exec) {
    if (arg_debug)
      printf("%s", "Child process is allowed to proceed by the sandbox\n");
    ret_value = (*original_execve)(allowed_exec, allowed_argv, envp);
  } else {
    errno = EACCES;
    ret_value = -1;
  }

  free(allowed_exec);
  free(forbidden_exec);

  if (allowed_argv)
    free(allowed_argv);

  if (forbidden_argv)
    free(forbidden_argv);

  return ret_value;
}

int execvpe(const char *file, char *const argv[], char *const envp[])
{
  typeof(execvpe) *original_execvpe = dlsym(RTLD_NEXT, "execvpe");
  if(!original_execvpe) {
    fprintf(stderr, "Critical: the child process is not properly loading the standard C library, and libexechelper cannot find the real implementation of execvpe(), aborting\n");
    return EACCES;
  }
  if (arg_debug)
    printf("Child process is attempting to execute (execvpe) binary name '%s'\n", file);

  char *path = exechelp_resolve_executable_path(file);
  if (!path) {
    if (arg_debug >= 2)
      printf("DEBUG: '%s' could not be resolved to any path, we expect execvpe to return ENOENT or a similar error.\n", file);
    errno = ENOENT;
    return -1;
  }

  if (access(path, R_OK | X_OK)) {
    if (arg_debug)
      printf("Child process cannot execute '%s' as this binary cannot be accessed, aborting\n", path);
    free(path);
    // we keep the errno from access
    return -1;
  }

  char *allowed_exec = NULL, *forbidden_exec = NULL;
  char **allowed_argv = NULL, **forbidden_argv = NULL;
  int ret_value = 0;

  exechelp_filter_forbidden_exec(path, argv, envp,
                                 &allowed_exec, &allowed_argv,
                                 &forbidden_exec, &forbidden_argv);

  /* First getting rid of the denied process/files because we know we will return from this call. */
  if (forbidden_exec) {
    if (!allowed_exec && arg_debug)
      printf("Child process delegating the execution of '%s' to the sandbox\n", path); 
    else if (arg_debug)
      printf("Child process must delegate the execution of some parameters of '%s' to the sandbox\n", path);

    if (exechelp_send_command(forbidden_exec, forbidden_argv) != -1) {
      if (arg_debug)
        printf("Child process's execvpe() system call successfully hijacked for sandbox to take over\n");
    } else if (arg_debug)
      printf("Error: Child process's system call should have been sent to the sandbox helper for execution, but an error occurred\n");
  }

  /* Then executing the allowed process with the allowed parameters. We might not return.
   */
  if (allowed_exec) {
    if (arg_debug)
      printf("%s", "Child process is allowed to proceed by the sandbox\n");
    ret_value = (*original_execvpe)(allowed_exec, allowed_argv, envp);
  } else {
    errno = EACCES;
    ret_value = -1;
  }

  free(allowed_exec);
  free(forbidden_exec);

  if (allowed_argv)
    free(allowed_argv);

  if (forbidden_argv)
    free(forbidden_argv);

  free(path);
  return ret_value;
}

int fexecve(int fd, char *const argv[], char *const envp[])
{
  if(fd<0) {
    errno = EINVAL;
    return -1;
  }

  size_t len = strlen("/proc/self/fd/") + 48;
  char *fdpath = malloc(sizeof(char) * len);
  snprintf(fdpath, len, "/proc/self/fd/%d", fd);

  char *path = exechelp_coreutils_areadlink_with_size(fdpath, 2048);
  free(fdpath);
  if(!path) {
    errno = EINVAL;
    return -1;
  }

  if (access(path, R_OK | X_OK)) {
    if (arg_debug)
      printf("Child process cannot execute '%s' as this binary cannot be accessed, aborting\n", path);
    free(path);
    // we keep the errno from access
    return -1;
  }

  typeof(execve) *original_execve = dlsym(RTLD_NEXT, "execve");
  if (arg_debug)
    printf("Child process is attempting to execute (fexecve) file descriptor '%s' (%d)\n", path, fd);

  char *allowed_exec = NULL, *forbidden_exec = NULL;
  char **allowed_argv = NULL, **forbidden_argv = NULL;
  int ret_value = 0;

  exechelp_filter_forbidden_exec(path, argv, envp,
                                 &allowed_exec, &allowed_argv,
                                 &forbidden_exec, &forbidden_argv);

  /* First getting rid of the denied process/files because we know we will return from
   * this call.
   */
  if (forbidden_exec) {
    if (!allowed_exec && arg_debug)
      printf("Child process delegating the execution of '%s' to the sandbox\n", path); 
    else if (arg_debug)
      printf("Child process must delegate the execution of some parameters of '%s' to the sandbox\n", path); 

    if (exechelp_send_command(forbidden_exec, forbidden_argv) != -1) {
      if (arg_debug)
        printf("Child process's fexecve() system call successfully hijacked for sandbox to take over\n");
    } else if (arg_debug)
      printf("Error: Child process's system call should have been sent to the sandbox helper for execution, but an error occurred\n");
  }

  /* Then executing the allowed process with the allowed parameters. We might not return.
   */
  if (allowed_exec) {
    if (arg_debug)
      printf("%s", "Child process is allowed to proceed by the sandbox\n");
    ret_value = (*original_execve)(allowed_exec, allowed_argv, envp);
  } else {
    errno = EACCES;
    ret_value = -1;
  }

  free(allowed_exec);
  free(forbidden_exec);

  if (allowed_argv)
    free(allowed_argv);

  if (forbidden_argv)
    free(forbidden_argv);

  free(path);
  return ret_value;
}

