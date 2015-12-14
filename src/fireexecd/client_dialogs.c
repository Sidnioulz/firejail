/*
 * Copyright (C) 2015 Steve Dodier-Lazaro (sidnioulz@gmail.com)
 *
 * This file is part of firejail project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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

#include "fireexecd.h"
#include "exechelp-dialog.h"
#include "../include/exechelper.h"
#include "../include/exechelper-datatypes.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_GTK
#include <glib.h>
#include <gtk/gtk.h>
#endif

#ifdef GDK_WINDOWING_X11
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#endif

#define   EH_RESPONSE_UNSANDBOXED               101
#define   EH_RESPONSE_SANDBOXED_DEFAULT         102
#define   EH_RESPONSE_SANDBOXED_ORIGINAL        103
#define   EH_RESPONSE_SANDBOXED_BACKUP_HANDLER  104


void client_gtk_init(void) {
  static int initd = 0;

  if (!initd) {
    initd = 1;
    gtk_init(0, NULL);
  }
}

int client_notify(fireexecd_client_t *cli, const char *icon, const char *header, const char *body) {
  DBGENTER(cli?cli->pid:-1, "client_notify");
  char *notification;
  int   ret = -1;

  if (asprintf(&notification, "notify-send -i %s \"%s\" \"%s\"", icon, header, body) == -1) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
      DBGLEAVE(cli?cli->pid:-1, "client_notify");
      return ret;
  }

  ret = system(notification);
  free(notification);
  DBGLEAVE(cli?cli->pid:-1, "client_notify");
  return ret;
}

static void client_dialog_run(fireexecd_client_t *cli,
                              const char *caller_command,
                              const char *command,
                              char *argv[],
                              const size_t argc,
                              char **forbidden_files,
                              char **files_to_whitelist,
                              const char *reason,
                              ExecHelpProtectedFileHandler *backup_command,
                              ExecHelpProtectedFileHandler *valid_handler) {
  DBGENTER(cli?cli->pid:-1, "client_dialog_run");
#ifdef HAVE_GTK
  GtkWidget *dialog;
  GtkWidget *content_area;
  GtkWidget *widget, *box;
  DialogExecData *d;
  client_gtk_init();
  char *message, *label;

  const char *command_name = strrchr(command, '/');
  if (command_name)
    command_name++;
  else
    command_name = command;

  // build the DialogExecData struct
  d = exechelp_malloc0(sizeof(DialogExecData));
  d->caller_command = caller_command ? strdup(caller_command) : NULL;
  d->command        = command ? strdup(command) : NULL;
  d->argv           = string_list_copy(argv);
  d->argc           = argc;

  // construct dialog
  dialog = exechelp_dialog("Sandbox App Launch Protection", TRUE);
  content_area = exechelp_dialog_get_content_area(EH_DIALOG(dialog));

  // create the dialog message
  if (asprintf(&message, "<b>%s was prevented from running %s.</b>\n\n%s\nHow would you like to run %s?", caller_command, command_name, reason, command_name) == -1) {
      DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
      DBGLEAVE(cli?cli->pid:-1, "client_dialog_run");
      return;
  }
  widget = gtk_label_new(message);
  free(message);
  gtk_label_set_use_markup(GTK_LABEL(widget), TRUE);
  gtk_label_set_line_wrap(GTK_LABEL(widget), TRUE);

  // pack up the widget
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 12);
  gtk_box_pack_start (GTK_BOX (content_area), box, TRUE, TRUE, 12);

  // Run sandboxed button
  widget = exechelp_dialog_button(EH_DIALOG(dialog), EH_RESPONSE_SANDBOXED_DEFAULT, "firejail-new", NULL, "<b>Run in _New Sandbox</b>\n<small>(profile: <i>auto</i>)</small>");

  // Open Files in default handler
  if (valid_handler) {
    if (asprintf(&label, "<b>Run in _Compatible Protected File Handler</b>\n<small>(will run in the protected file's most compatible handler <i>%s</i> under profile <i>%s</i>)</small>", valid_handler->handler_path, valid_handler->profile_name) == -1) {
        DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
        DBGLEAVE(cli?cli->pid:-1, "client_dialog_run");
        return;
    }

    char *handler_icon = strrchr(valid_handler->handler_path, '/');
    if (handler_icon)
      handler_icon++;
    else
      handler_icon = valid_handler->handler_path;

    widget = exechelp_dialog_button(EH_DIALOG(dialog), EH_RESPONSE_SANDBOXED_BACKUP_HANDLER, handler_icon, "preferences-desktop-default-applications", label);
    free(label);
  }

  // Run in original box button, only propose if previous box was named
  if (cli->name) {
    if (asprintf(&label, "<b>Run in _Original Sandbox</b>\n<small>(will run in sandbox <i>%s</i> under profile <i>%s</i>)</small>", cli->name, cli->profile) == -1) {
        DBGERR("[%d]\t\e[01;40;101mERROR:\e[0;0m failed to call asprintf() (error: %s)\n", cli->pid, strerror(errno));
        DBGLEAVE(cli?cli->pid:-1, "client_dialog_run");
        return;
    }

    widget = exechelp_dialog_button(EH_DIALOG(dialog), EH_RESPONSE_SANDBOXED_ORIGINAL, "firejail-link", NULL, label);
    free(label);
  }

  // Run unsandboxed button
  widget = exechelp_dialog_button(EH_DIALOG(dialog), EH_RESPONSE_UNSANDBOXED, "system-run", NULL, "<b>Run _Unsandboxed</b>\n");

  // and show it to the world...
  gtk_widget_show_all(GTK_WIDGET(dialog));
  int ret = exechelp_dialog_run(EH_DIALOG(dialog));
  exechelp_dialog_destroy(EH_DIALOG(dialog));

  if (ret == EH_RESPONSE_SANDBOXED_DEFAULT) {
    int ret = client_execute_sandboxed(cli, command, argv, argc, NULL, NULL, TRUE, TRUE, files_to_whitelist);
  } else if (ret == EH_RESPONSE_SANDBOXED_ORIGINAL) {
    int ret = client_execute_sandboxed(cli, command, argv, argc, cli->profile, cli->name, TRUE, TRUE, files_to_whitelist);
  } else if (ret == EH_RESPONSE_SANDBOXED_BACKUP_HANDLER) {
    argv[0] = strdup(valid_handler->handler_path);
    int ret = client_execute_sandboxed(cli, valid_handler->handler_path, argv, argc, valid_handler->profile_name, NULL, TRUE, TRUE, files_to_whitelist);
  } else if (ret == EH_RESPONSE_UNSANDBOXED) {
    int ret = client_execute_unsandboxed(cli, command, argv);
  }

#endif
  DBGLEAVE(cli?cli->pid:-1, "client_dialog_run");
}

void client_dialog_run_without_reason(fireexecd_client_t *cli,
                                      const char *caller_command,
                                      const char *command,
                                      char *argv[],
                                      const size_t argc,
                                      char **files_to_whitelist) {
  DBGENTER(cli?cli->pid:-1, "client_dialog_run_without_reason");
  client_dialog_run(cli, caller_command, command, argv, argc, NULL, files_to_whitelist,
    "The reason why the command was blocked could not be determined.\n"
    "This could be caused by an inconsistency in the security policy or a bug in the sandbox.\n",
    NULL, NULL);
  DBGLEAVE(cli?cli->pid:-1, "client_dialog_run_without_reason");
}

void client_dialog_run_incompatible(fireexecd_client_t *cli,
                                    const char *caller_command,
                                    const char *command,
                                    char *argv[],
                                    const size_t argc,
                                    char **forbidden_files,
                                    char **files_to_whitelist,
                                    ExecHelpProtectedFileHandler *backup_command,
                                    ExecHelpProtectedFileHandler *valid_handler) {
  DBGENTER(cli?cli->pid:-1, "client_dialog_run_without_reason");
  client_dialog_run(cli, caller_command, command, argv, argc, forbidden_files, files_to_whitelist,
    "The sandboxed app attempted to open one or more protected files with an"
    "incompatible application. Only applications in the protected file's policy"
    "can be used.\n", backup_command, valid_handler);
  DBGLEAVE(cli?cli->pid:-1, "client_dialog_run_without_reason");
}
