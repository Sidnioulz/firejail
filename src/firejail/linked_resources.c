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
#include <limits.h>
#include <stdlib.h>

char *get_linked_apps_for_client(void) {
  char *assocs = exechelp_extract_associations_for_binary(cfg.command_name);

  // always associated with self?
  // if (!assocs)
  //   assocs = exechelp_resolve_executable_path(cfg.command_name);

  return assocs;
}

int is_command_linked_for_client(const char *command) {
  char *linked = get_linked_apps_for_client();
  if (!linked)
    return 0;

/*
  // use code below if you want to allow command names rather than only full paths
  char *real = exechelp_resolve_executable_path(command);
  if (!real) {
    fprintf(stderr, "Error: could not find a real path for command (%s)\n", command);
    return 0;
  }
*/

  int result = strstr(linked, command) != NULL;
//  int result = strstr(linked, real) != NULL;
//  free(real);
  free(linked);

  return result;
}
