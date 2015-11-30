/*
 *  2015 (c) Steve Dodier-Lazaro <sidnioulz@gmail.com>
 *  This file is part of ExecHelper.
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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../include/exechelper.h"

// This file is only used for the sake of exporting things that are normally
// present in firejail or fireexecd like arg_debug
int arg_debug = 0;

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
