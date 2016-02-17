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

#include <dlfcn.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "../include/common.h"
#include "../include/exechelper.h"
#include "../include/exechelper-logger.h"

int arg_debug = DEBUGLVL;

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

// report sandbox parameters to xfwm4
typedef Window (* xcw_func)(Display *, Window, int, int, unsigned int, unsigned int, unsigned int, int,
                            unsigned int, Visual *, unsigned long, XSetWindowAttributes *);

Window XCreateWindow(Display *display, Window parent, int x, int y, unsigned int width, unsigned int height, unsigned int border_width, int depth,
                     unsigned int class, Visual *visual, unsigned long valuemask, XSetWindowAttributes *attributes)
{
  static xcw_func xcw = NULL;
  if (!xcw) {
      xcw = dlsym(RTLD_NEXT, "XCreateWindow");
  }

  static Atom container_atom = 0, type_atom = 0, name_atom = 0, ws_atom = 0;
  if (!container_atom) {
    container_atom = XInternAtom(display, "CONTAINER", False);
    type_atom      = XInternAtom(display, EXECHELP_SANDBOX_TYPE_ENV, False);
    name_atom      = XInternAtom(display, EXECHELP_SANDBOX_NAME_ENV, False);
    ws_atom        = XInternAtom(display, EXECHELP_SANDBOX_LOCK_WS_ENV, False);
  }

  Window window = xcw(display, parent, x, y, width, height, border_width, depth, class, visual, valuemask, attributes);

  char *type = getenv(EXECHELP_SANDBOX_TYPE_ENV);
  if (!type)
    type = "untrusted";

  char *name = getenv(EXECHELP_SANDBOX_NAME_ENV);
  if (!name)
    name = "";

  char *ws = getenv(EXECHELP_SANDBOX_LOCK_WS_ENV);
  if (!ws)
    ws = "";

  XChangeProperty(display, window, container_atom, XA_STRING, 8, PropModeReplace, (unsigned char *) "firejail", 9);
  XChangeProperty(display, window, type_atom, XA_STRING, 8, PropModeReplace, (unsigned char *) type, strlen(type)+1);
  XChangeProperty(display, window, name_atom, XA_STRING, 8, PropModeReplace, (unsigned char *) name, strlen(name)+1);
  XChangeProperty(display, window, ws_atom, XA_STRING, 8, PropModeReplace, (unsigned char *) ws, strlen(ws)+1);

  return window;
}
