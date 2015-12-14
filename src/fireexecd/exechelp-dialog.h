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

#ifndef __EH_DIALOG_H__
#define __EH_DIALOG_H__
#ifdef HAVE_GTK

#include <gtk/gtk.h>

typedef struct _ExecHelpDialogClass ExecHelpDialogClass;
typedef struct _ExecHelpDialog      ExecHelpDialog;

#define EH_TYPE_DIALOG            (exechelp_dialog_get_type ())
#define EH_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EH_TYPE_DIALOG, ExecHelpDialog))
#define EH_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EH_TYPE_DIALOG, ExecHelpDialogClass))
#define EH_IS_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EH_TYPE_DIALOG))
#define EH_IS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EH_TYPE_DIALOG))
#define EH_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EH_TYPE_DIALOG, ExecHelpDialogClass))

GType           exechelp_dialog_get_type           (void) G_GNUC_CONST;
GtkWidget      *exechelp_dialog                    (const gchar      *title,
                                                    gboolean          has_cancel);
GtkWidget *     exechelp_dialog_get_content_area   (ExecHelpDialog   *dialog);
gint            exechelp_dialog_run                (ExecHelpDialog   *dialog);
void            exechelp_dialog_destroy            (ExecHelpDialog   *dialog);
GtkWidget      *exechelp_dialog_button             (ExecHelpDialog   *dialog,
                                                    gint              response,
                                                    const gchar      *title,
                                                    const gchar      *icon_name,
                                                    const gchar      *icon_name_fallback);
#endif /* HAVE_GTK */
#endif
