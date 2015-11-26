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

#ifndef __EH_EXECHELP_H__
#define __EH_EXECHELP_H__

#include <stdio.h>
#include "exechelper-datatypes.h"

/* Debug macros */
#ifndef DEBUGLVL
#define DEBUGLVL 2
#endif

extern int arg_debug;

/* Constants */
#define EXECHELP_NULL_BINARY_PATH         "/dev/null"

#define EXECHELP_LINKED_APPS              "linked-apps.list"
#define EXECHELP_PROTECTED_APPS           "protected-apps.list"
#define EXECHELP_WHITELIST_FILES          "whitelist-files.list"
#define EXECHELP_WHITELIST_APPS           "whitelist-apps.list"
#define EXECHELP_PROTECTED_FILES          "protected-files.list"
#define EXECHELP_PROTECTED_APPS_BIN       "protected-apps.bin"
#define EXECHELP_PROTECTED_FILES_BIN      "protected-files.bin"

#define EXECHELP_CLIENT_ROOT              "/etc/firejail/self"
#define EXECHELP_LINKED_APPS_PATH         EXECHELP_CLIENT_ROOT"/"EXECHELP_LINKED_APPS
#define EXECHELP_PROTECTED_APPS_PATH      EXECHELP_CLIENT_ROOT"/"EXECHELP_PROTECTED_APPS
#define EXECHELP_PROTECTED_FILES_PATH     EXECHELP_CLIENT_ROOT"/"EXECHELP_PROTECTED_FILES
#define EXECHELP_WHITELIST_APPS_PATH      EXECHELP_CLIENT_ROOT"/"EXECHELP_WHITELIST_APPS
#define EXECHELP_WHITELIST_FILES_PATH     EXECHELP_CLIENT_ROOT"/"EXECHELP_WHITELIST_FILES

#define EXECHELP_LD_PRELOAD_PATH           "/usr/local/lib/firejail/libexechelper.so"

#define EXECHELP_RUN_DIR                  "/run/firejail"
#define EXECHELP_REGISTRATION_SOCKET      "/run/firejail/execd-register.sock"
#define EXECHELP_COMMANDS_SOCKET          "EXECHELP_COMMANDS_SOCKET"
#define EXECHELP_EXECUTION_POLICY         "EXECHELP_EXECUTION_POLICY"
// 256 for each socket path, 12 for the pid, 12 for the message syntax
#define EXECHELP_REGISTRATION_MSGLEN      (512 + 12 + 20)
#define EXECHELP_COMMAND_MAXLEN           92000
#define EXECHELP_POLICY_LINE_MAX_READ     8192

#define EXECHELP_PROFILE_ANY              "*"
#define EXECHELP_PROFILE_NONE             "UNSANDBOXED"

#define EXECHELP_SANDBOX_TYPE_ENV         "FIREJAIL_SANDBOX_TYPE"
#define EXECHELP_SANDBOX_NAME_ENV         "FIREJAIL_SANDBOX_NAME"
#define EXECHELP_DEBUG_ENV                "EXECHELP_DEBUG"

#define DEFAULT_USER_PROFILE	"generic"
#define DEFAULT_ROOT_PROFILE	"server"

/* Execution policy */
typedef enum _ExecHelpExecutionPolicy {
  LINKED_APP = 1,
  UNSPECIFIED = 1 << 1,
  SANDBOX_PROTECTED = 1 << 2,
  SANDBOX_ITSELF = 1 << 3
} ExecHelpExecutionPolicy;
#define EXECHELP_DEFAULT_POLICY           LINKED_APP | UNSPECIFIED

/* execd / exechelper IPC */
// build /run/firejail directory
void exechelp_build_run_dir(void);
void exechelp_make_socket_files(void);

/* General utilities */
char *exechelp_read_list_from_file(const char *file_path);
int exechelp_str_has_prefix_on_sep(const char *str, const char *prefix, const char sep);
int exechelp_file_list_contains_path(const char *list, const char *real, char **prefix);
ExecHelpExecutionPolicy *exechelp_targets_sandbox_protected_file(const char *filepath, const char *target, char *const argv[], int whitelist, char **forbiddenpaths);
char *exechelp_resolve_path(const char *target);

/* Binary association structure */
typedef struct _ExecHelpBinaryAssociations {
  ExecHelpSList     *assoc;
  ExecHelpHashTable *index;
} ExecHelpBinaryAssociations;

ExecHelpBinaryAssociations *exechelp_get_binary_associations();
ExecHelpSList *exechelp_get_associations_for_main_binary(ExecHelpBinaryAssociations *assoc, const char *mainkey);
ExecHelpSList *exechelp_get_associations_for_arbitrary_binary(ExecHelpBinaryAssociations *assoc, const char *key);
int exechelp_is_associated_app(const char *caller, const char *callee);
char *exechelp_extract_associations_for_binary(const char *receiving_binary);

/* Memory functions */
void *exechelp_malloc0(size_t size);
void *exechelp_memdup (const void *mem, unsigned int byte_size);

/* realpath - print the resolved path
 * Copyright (C) 2011-2015 Free Software Foundation, Inc.
 * Written by Pádraig Brady. */
char *exechelp_coreutils_areadlink_with_size(char const *file, size_t size);
char *exechelp_coreutils_realpath (const char *fname);

#endif /* __EH_EXECHELP_H__ */
