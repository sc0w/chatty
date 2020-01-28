/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-account"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#define HANDY_USE_UNSTABLE_API
#include <handy.h>
#include "purple.h"
#include "chatty-buddy-list.h"
#include "dialogs/chatty-dialogs.h"
#include "dialogs/chatty-settings-dialog.h"
#include "dialogs/chatty-new-chat-dialog.h"
#include "chatty-icons.h"
#include "chatty-window.h"
#include "chatty-utils.h"
#include "chatty-account.h"
#include "users/chatty-pp-account.h"
#include "chatty-purple-init.h"

struct auth_request
{
  void                                *data;
  char                                *username;
  char                                *alias;
  PurpleAccount                       *account;
  gboolean                             add_buddy_after_auth;
  PurpleAccountRequestAuthorizationCb  auth_cb;
  PurpleAccountRequestAuthorizationCb  deny_cb;
};


static void
chatty_account_notify_added (PurpleAccount *account,
                             const char    *remote_user,
                             const char    *id,
                             const char    *alias,
                             const char    *msg)
{
  GtkWidget  *dialog;
  GtkWindow  *window;

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
  dialog = gtk_message_dialog_new (window,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_OK,
                                   _("Contact added"));


  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG(dialog),
                                            _("User %s has added %s to the contacts"),
                                            remote_user,
                                            id);

  gtk_dialog_set_default_response (GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
  gtk_window_set_position (GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

  gtk_dialog_run (GTK_DIALOG(dialog));

  gtk_widget_destroy (dialog);
}


static void
chatty_account_free_auth_request (struct auth_request *ar)
{
  g_free (ar->username);
  g_free (ar->alias);
  g_free (ar);
}


static void
chatty_account_authorize_and_add (struct auth_request *ar)
{
  ar->auth_cb(ar->data);

  if (ar->add_buddy_after_auth) {
    purple_blist_request_add_buddy (ar->account, ar->username, NULL, ar->alias);
  }
}


static void
chatty_account_deny_authorize (struct auth_request *ar)
{
  ar->deny_cb(ar->data);
}


static void *
chatty_account_request_authorization (PurpleAccount *account,
                                      const char    *remote_user,
                                      const char    *id,
                                      const char    *alias,
                                      const char    *message,
                                      gboolean       on_list,
                                      PurpleAccountRequestAuthorizationCb auth_cb,
                                      PurpleAccountRequestAuthorizationCb deny_cb,
                                      void          *user_data)
{
  GtkWidget           *dialog;
  GtkWindow           *window;
  struct auth_request *ar;
  int                  response;

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
  dialog = gtk_message_dialog_new (window,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("Authorize %s?"),
                                   (alias != NULL ? alias : remote_user));

  gtk_dialog_add_buttons (GTK_DIALOG(dialog),
                          _("Reject"),
                          GTK_RESPONSE_CANCEL,
                          _("Accept"),
                          GTK_RESPONSE_OK,
                          NULL);

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG(dialog),
                                            _("Add %s to contact list"),
                                            remote_user);

  gtk_dialog_set_default_response (GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
  gtk_window_set_position (GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

  ar = g_new0 (struct auth_request, 1);
  ar->auth_cb = auth_cb;
  ar->deny_cb = deny_cb;
  ar->data = user_data;
  ar->username = g_strdup (remote_user);
  ar->alias = g_strdup (alias);
  ar->account = account;
  ar->add_buddy_after_auth = !on_list;

  response = gtk_dialog_run (GTK_DIALOG(dialog));

  if (response == GTK_RESPONSE_OK) {
    chatty_account_authorize_and_add (ar);
  } else {
    chatty_account_deny_authorize (ar);
  }

  gtk_widget_destroy (dialog);

  chatty_account_free_auth_request (ar);

  g_debug ("Request authorization user: %s alias: %s", remote_user, alias);

  return NULL;
}


static void
chatty_account_request_close (void *ui_handle)
{
  gtk_widget_destroy( GTK_WIDGET(ui_handle));
}


static void
chatty_account_request_add (PurpleAccount *account,
                            const char    *remote_user,
                            const char    *id,
                            const char    *alias,
                            const char    *msg)
{
  PurpleConnection *gc;

  gc = purple_account_get_connection (account);

  if (g_list_find (purple_connections_get_all (), gc)) {
    purple_blist_request_add_buddy (account, remote_user, NULL, alias);
  }

  g_debug ("chatty_account_request_add");
}


static PurpleAccountUiOps ui_ops =
{
  chatty_account_notify_added,
  NULL,
  chatty_account_request_add,
  chatty_account_request_authorization,
  chatty_account_request_close,
  NULL,
  NULL,
  NULL,
  NULL
};


PurpleAccountUiOps *
chatty_accounts_get_ui_ops (void)
{
  return &ui_ops;
}
