/* $Id$ */
/*-
 * Copyright (c) 2004-2006 Benedikt Meurer <benny@xfce.org>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 */

#ifdef HAVE_GTK
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include "xfsm-fadeout.h"



struct _XfsmFadeout
{
  GdkWindow *window;
};



XfsmFadeout*
xfsm_fadeout_new (GdkDisplay *display)
{
  GdkWindowAttr  attr;
  XfsmFadeout   *fadeout;
  GdkWindow     *root;
  GdkCursor     *cursor;
  cairo_t       *cr;
  gint           width;
  gint           height;
  #if 0
  GdkPixbuf     *root_pixbuf;
  GdkPixmap     *backbuf;
  #endif
  GdkScreen     *gdk_screen;
  GdkWindow     *window;
  GdkRGBA       black = { 0, };

  fadeout = g_slice_new0 (XfsmFadeout);

  cursor = gdk_cursor_new_for_display (display, GDK_WATCH);

  attr.x = 0;
  attr.y = 0;
  attr.event_mask = 0;
  attr.wclass = GDK_INPUT_OUTPUT;
  attr.window_type = GDK_WINDOW_TEMP;
  attr.cursor = cursor;
  attr.override_redirect = TRUE;

  gdk_screen = gdk_display_get_screen (display, 0);

  root = gdk_screen_get_root_window (gdk_screen);
  width = gdk_window_get_width (root);
  height = gdk_window_get_height (root);

  attr.width = width;
  attr.height = height;
  window = gdk_window_new (root, &attr, GDK_WA_X | GDK_WA_Y
                           | GDK_WA_NOREDIR | GDK_WA_CURSOR);

  if (gdk_screen_is_composited (gdk_screen)
      && gdk_screen_get_rgba_visual (gdk_screen) != NULL)
    {
      /* transparent black window */
      black.alpha = 1;
      gdk_window_set_background_rgba (window, &black);
      gdk_window_set_opacity (window, 0.5);
    }
  else
    {
      cairo_pattern_t *pattern = cairo_pattern_create_rgba (0, 0, 0, 0.5);
      gdk_window_set_background_pattern (window, pattern);
      cairo_pattern_destroy (pattern);
    }

  fadeout->window = window;
  gdk_window_show(fadeout->window);

  g_object_unref (cursor);

  return fadeout;
}



void
xfsm_fadeout_clear (XfsmFadeout *fadeout)
{
  if (fadeout != NULL)
    gdk_window_hide(fadeout->window);
}



void
xfsm_fadeout_destroy (XfsmFadeout *fadeout)
{
  gdk_window_hide(fadeout->window);
  gdk_window_destroy(fadeout->window);
  g_slice_free (XfsmFadeout, fadeout);
}

#endif
