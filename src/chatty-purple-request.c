/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include <glib/gi18n.h>
#include "chatty-purple-request.h"


static void
cb_action_response (GtkDialog         *dialog,
                    gint               id,
                    ChattyRequestData *data)
{
  if (id >= 0 && (gsize)id < data->cb_count && data->cbs[id] != NULL) {
    ((PurpleRequestActionCb)data->cbs[id])(data->user_data, id);
  }

  purple_request_close (PURPLE_REQUEST_INPUT, data);
}


static void *
chatty_request_action (const char         *title,
                       const char         *primary,
                       const char         *secondary,
                       int                 default_action,
                       PurpleAccount      *account,
                       const char         *who,
                       PurpleConversation *conv,
                       void               *user_data,
                       size_t              action_count,
                       va_list             actions)
{
  ChattyRequestData  *data;
  GtkWindow          *window;
  GtkWidget          *dialog;
  void              **buttons;
  gsize               i;

  if (account || conv || who) {
    // we only handle libpurple request-actions
    // for certificates
    return NULL;
  }

  data            = g_new0 (ChattyRequestData, 1);
  data->type      = PURPLE_REQUEST_ACTION;
  data->user_data = user_data;
  data->cb_count  = action_count;
  data->cbs       = g_new0 (GCallback, action_count);

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));

  dialog = gtk_message_dialog_new (window,
                                   GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_NONE,
                                   "%s", primary ? primary : title);

  g_signal_connect (G_OBJECT(dialog),
                    "response",
                    G_CALLBACK(cb_action_response),
                    data);

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG(dialog),
                                            "%s",
                                            secondary);

  buttons = g_new0 (void *, action_count * 2);

  for (i = 0; i < action_count * 2; i += 2) {
    buttons[(action_count * 2) - i - 2] = va_arg(actions, char *);
    buttons[(action_count * 2) - i - 1] = va_arg(actions, GCallback);
  }

  for (i = 0; i < action_count; i++) {
    gtk_dialog_add_button (GTK_DIALOG(dialog), buttons[2 * i], i);

    data->cbs[i] = buttons[2 * i + 1];
  }

  g_free (buttons);

  gtk_dialog_set_default_response (GTK_DIALOG(dialog), action_count - 1 - default_action);

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
  gtk_window_set_transient_for (GTK_WINDOW(dialog), window);

  gtk_widget_show_all (dialog);

  data->dialog = dialog;

  return data;
}


static void
chatty_close_request (PurpleRequestType  type,
                      void              *ui_handle)
{
  ChattyRequestData *data = (ChattyRequestData *)ui_handle;

  g_free(data->cbs);

  gtk_widget_destroy (data->dialog);

  g_free (data);
}


static PurpleRequestUiOps ops =
{
  NULL,
  NULL,
  chatty_request_action,
  NULL,
  NULL,
  chatty_close_request,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};


PurpleRequestUiOps *
chatty_request_get_ui_ops(void)
{
  return &ops;
}
