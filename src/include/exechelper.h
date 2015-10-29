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
#define DEBUGLVL 1
#endif

#define DEBUG(fmt, ...) \
    do { if (DEBUGLVL) fprintf(stderr, fmt, __VA_ARGS__); } while (0)

#define DEBUG2(fmt, ...) \
    do { if (DEBUGLVL>1) fprintf(stderr, fmt, __VA_ARGS__); } while (0)

/* Constants */
#define EXECHELP_NULL_BINARY_PATH         "/dev/null"
#define EXECHELP_MONITORED_EXEC_PATH      "/run/firejail/exechelper/"

#define EXECHELP_LINKED_APPS              "linked-apps.list"
#define EXECHELP_PROTECTED_APPS           "protected-apps.list"
#define EXECHELP_PROTECTED_FILES          "protected-files.list"

#define EXECHELP_CLIENT_ROOT              "/etc/firejail/self"
#define EXECHELP_LINKED_APPS_PATH         EXECHELP_CLIENT_ROOT"/"EXECHELP_LINKED_APPS
#define EXECHELP_PROTECTED_APPS_PATH      EXECHELP_CLIENT_ROOT"/"EXECHELP_PROTECTED_APPS
#define EXECHELP_PROTECTED_FILES_PATH     EXECHELP_CLIENT_ROOT"/"EXECHELP_PROTECTED_FILES

#define EXECHELP_LD_PRELOAD_PATH           "/usr/lib/libexechelper.so"

/* General utilities */
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
