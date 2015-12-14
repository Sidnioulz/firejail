/*-
 * Copyright (c) 2015  Steve Dodier-Lazaro <sidnioulz@gmail.org>
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
 *
 * Most of this file is a fork from xfce4-session/xfsm-logout-dialog.c, which
 * was written by Benedikt Meurer <benny@xfce.org> and is maintained by
 * Nick Schermer <nick@xfce.org>.
 *
 * Parts of the Xfce file where taken from gnome-session/logout.c, which
 * was written by Owen Taylor <otaylor@redhat.com>.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#ifdef HAVE_GTK

#include <gtk/gtk.h>
#include "xfsm-fadeout.h"
#include "exechelp-dialog.h"

#ifdef GDK_WINDOWING_X11
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#endif



#define BORDER   6
#define SHOTSIZE 64



static void       exechelp_dialog_finalize    (GObject          *object);
static void       exechelp_dialog_activate    (ExecHelpDialog   *dialog);



struct _ExecHelpDialogClass
{
  GtkDialogClass __parent__;
};

struct _ExecHelpDialog
{
  GtkDialog __parent__;
  GtkWidget   *content_area;
  XfsmFadeout *fadeout;
  gboolean     a11y;
};



G_DEFINE_TYPE (ExecHelpDialog, exechelp_dialog, GTK_TYPE_DIALOG)



/* code taken from Libxfce4ui, (c) 2015 the Xfce project - http://xfce.org */
static GdkScreen *
xfce_gdk_screen_get_active ()
{
  GdkDisplay       *display;
  gint              rootx, rooty;
  GdkScreen        *screen;
  GdkDeviceManager *manager;

  display = gdk_display_get_default ();
  manager = gdk_display_get_device_manager (display);
  gdk_device_get_position (gdk_device_manager_get_client_pointer (manager),
                           &screen, &rootx, &rooty);

  if (G_UNLIKELY (screen == NULL))
    {
      screen = gdk_screen_get_default ();
    }

  return screen;
}



static void
exechelp_dialog_class_init (ExecHelpDialogClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = exechelp_dialog_finalize;
}

static gboolean
check_escape(GtkWidget   *widget,
             GdkEventKey *event,
             gpointer     data)
{
  if (event->keyval == GDK_KEY_Escape)
    {
      gtk_dialog_response(GTK_DIALOG (widget), GTK_RESPONSE_CANCEL);
      return TRUE;
    }

  return FALSE;
}

static gboolean
focus_out_cb (GtkWidget *widget,
              GdkEventFocus *event,
              gpointer user_data)
{
  gtk_window_present(GTK_WINDOW(widget));
  return TRUE;
}

static void
exechelp_dialog_init (ExecHelpDialog *dialog)
{
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  gtk_window_set_skip_pager_hint (GTK_WINDOW(dialog), TRUE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW(dialog), TRUE);
  gtk_window_set_keep_above (GTK_WINDOW (dialog), TRUE);

  g_signal_connect(dialog, "key-press-event", G_CALLBACK(check_escape), NULL);
  g_signal_connect(dialog, "focus-out-event", G_CALLBACK(focus_out_cb), NULL);
}



static void
exechelp_dialog_finalize (GObject *object)
{
  ExecHelpDialog *dialog = EH_DIALOG (object);

  (*G_OBJECT_CLASS (exechelp_dialog_parent_class)->finalize) (object);
}



static void
exechelp_dialog_button_clicked (GtkWidget        *button,
                                   ExecHelpDialog *dialog)
{
  gint *val;

  val = g_object_get_data (G_OBJECT (button), "response");
  g_return_if_fail (val != NULL);

  gtk_dialog_response (GTK_DIALOG (dialog), *val);
}



GtkWidget *
exechelp_dialog_button (ExecHelpDialog *dialog,
                        gint              response,
                        const gchar      *icon_name,
                        const gchar      *icon_name_fallback,
                        const gchar      *title)
{
  GtkWidget    *button;
  GtkWidget    *vbox;
  GdkPixbuf    *pixbuf;
  GtkWidget    *image;
  GtkWidget    *label;
  static gint   icon_size = 0;
  gint          w, h;
  gint         *val;
  GtkIconTheme *icon_theme;

  g_return_val_if_fail (EH_IS_DIALOG (dialog), NULL);

  button = gtk_button_new ();
  val = g_new0 (gint, 1);
  *val = response;
  g_object_set_data_full (G_OBJECT (button), "response", val, g_free);
  g_signal_connect (G_OBJECT (button), "clicked",
      G_CALLBACK (exechelp_dialog_button_clicked), dialog);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, BORDER);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);
  gtk_container_add (GTK_CONTAINER (button), vbox);
  gtk_widget_show (vbox);

  if (G_UNLIKELY (icon_size == 0))
    {
      if (gtk_icon_size_lookup (GTK_ICON_SIZE_DND, &w, &h))
        icon_size = MAX (w, h);
      else
        icon_size = 32;
    }

  icon_theme = gtk_icon_theme_get_for_screen (gtk_window_get_screen (GTK_WINDOW (dialog)));
  pixbuf = gtk_icon_theme_load_icon (icon_theme, icon_name, icon_size, 0, NULL);
  if (G_UNLIKELY (pixbuf == NULL))
    {
      pixbuf = gtk_icon_theme_load_icon (icon_theme, icon_name_fallback,
                                         icon_size, GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                         NULL);
    }

  image = gtk_image_new_from_pixbuf (pixbuf);
  gtk_box_pack_start (GTK_BOX (vbox), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  label = gtk_label_new (NULL);
  gtk_label_set_markup_with_mnemonic (GTK_LABEL (label), title);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  if (dialog->content_area)
    gtk_container_add (GTK_CONTAINER (dialog->content_area), button);

  return button;
}



static void
exechelp_dialog_activate (ExecHelpDialog *dialog)
{
  g_return_if_fail (EH_IS_DIALOG (dialog));
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}



gint
exechelp_dialog_run (ExecHelpDialog *dialog)
{
  g_return_val_if_fail(EH_IS_DIALOG(dialog), GTK_RESPONSE_REJECT);

  gboolean          grab_input = !dialog->a11y;
  GdkWindow        *window;
  GdkDisplay       *display;
  GdkDeviceManager *device_manager;
  GdkDevice        *device;
  gint              ret;

  if (grab_input)
    {
      gtk_widget_show_now (GTK_WIDGET (dialog));

      window = gtk_widget_get_window (GTK_WIDGET (dialog));
      display = gdk_display_get_default ();
      device_manager = gdk_display_get_device_manager (display);
      device = gdk_device_manager_get_client_pointer (device_manager);

      if (gdk_device_grab (device, window, GDK_OWNERSHIP_NONE,
                           FALSE, GDK_ALL_EVENTS_MASK, NULL,
                           GDK_CURRENT_TIME)
          != GDK_GRAB_SUCCESS)
        g_critical ("Failed to grab the keyboard for logout window");

#ifdef GDK_WINDOWING_X11
      /* force input to the dialog */
      gdk_error_trap_push ();
      XSetInputFocus (GDK_DISPLAY_XDISPLAY(display),
                      gdk_x11_window_get_xid (window),
                      RevertToParent, CurrentTime);
      gdk_error_trap_pop_ignored ();
#endif
    }

  ret = gtk_dialog_run (GTK_DIALOG (dialog));

  if (grab_input)
    gdk_device_ungrab (device, GDK_CURRENT_TIME);

  return ret;
}

GtkWidget *
exechelp_dialog_get_content_area(ExecHelpDialog *dialog)
{
  g_return_val_if_fail(EH_IS_DIALOG(dialog), NULL);
  return dialog->content_area;
}

void
exechelp_dialog_destroy (ExecHelpDialog *dialog)
{
  g_return_if_fail(EH_IS_DIALOG(dialog));

  if (dialog->fadeout != NULL)
    xfsm_fadeout_destroy (dialog->fadeout);

  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  gtk_widget_hide(GTK_WIDGET(dialog));
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gdk_threads_leave ();
  
GMainContext *cont =g_main_loop_get_context (loop);
  while(g_main_context_pending(cont)) {
    g_main_context_iteration(cont, FALSE);
  }
//  g_main_loop_run (loop);
  gdk_threads_enter ();
  G_GNUC_END_IGNORE_DEPRECATIONS
  g_main_loop_unref (loop);
  gtk_widget_destroy(GTK_WIDGET(dialog));
}

GtkWidget *
exechelp_dialog (const gchar *title,
                 gboolean has_cancel)
{
  gint              result;
  GtkWidget        *hidden;
  gboolean          a11y;
  GtkWidget        *dialog;
  GtkWidget        *label;
  GtkWidget        *separator;
  PangoAttrList    *attrs;
  GdkScreen        *screen;
  XfsmFadeout      *fadeout = NULL;
  GdkDisplay       *display;
  GdkDeviceManager *device_manager;
  GdkDevice        *device;
  GtkWidget        *content_area;
  GtkWidget        *box;

  /* check if accessibility is enabled */
  screen = xfce_gdk_screen_get_active ();
  hidden = gtk_invisible_new_for_screen (screen);
  gtk_widget_show (hidden);
  a11y = GTK_IS_ACCESSIBLE (gtk_widget_get_accessible (hidden));

  if (G_LIKELY (!a11y))
    {
      /* wait until we can grab the keyboard, we need this for
       * the dialog when running it */
      for (;;)
        {
          display = gdk_display_get_default ();
          device_manager = gdk_display_get_device_manager (display);
          device = gdk_device_manager_get_client_pointer (device_manager);

          if (gdk_device_grab (device, gtk_widget_get_window (hidden),
                               GDK_OWNERSHIP_NONE, FALSE, GDK_ALL_EVENTS_MASK,
                               NULL, GDK_CURRENT_TIME) == GDK_GRAB_SUCCESS)
            {
              gdk_device_ungrab (device, GDK_CURRENT_TIME);
              break;
            }

          g_usleep (G_USEC_PER_SEC / 20);
        }

      /* display fadeout */
      //fadeout = xfsm_fadeout_new (gdk_screen_get_display (screen));
      gdk_flush ();

      dialog = g_object_new (EH_TYPE_DIALOG,
                             "type", GTK_WINDOW_POPUP,
                             "screen", screen, NULL);

      gtk_widget_realize (dialog);
      gdk_window_set_override_redirect (gtk_widget_get_window (dialog), TRUE);
      gdk_window_raise (gtk_widget_get_window (dialog));
    }
  else
    {
      dialog = g_object_new (EH_TYPE_DIALOG,
                             "decorated", !a11y,
                             "screen", screen, NULL);

      gtk_window_set_keep_above (GTK_WINDOW (dialog), TRUE);
      atk_object_set_role (gtk_widget_get_accessible (dialog), ATK_ROLE_ALERT);
    }

//  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  if (has_cancel)
    gtk_dialog_add_button(GTK_DIALOG(dialog), "_Cancel", GTK_RESPONSE_REJECT);

  // stuff a basic vbox in there, inside of which we'll pack our lines
  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (content_area), box);

  // set up an alternative to the title since we're most certainly not decorated
  gtk_window_set_title(GTK_WINDOW(dialog), title);
  label = gtk_label_new (title);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 12);
  gtk_widget_show (label);

  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_scale_new (PANGO_SCALE_LARGE));
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  pango_attr_list_unref (attrs);

  separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start (GTK_BOX (box), separator, FALSE, TRUE, 0);
  gtk_widget_show (separator);

  (EH_DIALOG(dialog))->a11y = a11y;
  (EH_DIALOG(dialog))->fadeout = fadeout;
  (EH_DIALOG(dialog))->content_area = box;
  gtk_widget_destroy (hidden);

  return dialog;
}

#endif
