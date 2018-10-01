/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include "purple.h"
#include "chatty-window.h"
#include "chatty-purple-init.h"


typedef struct
{
  PurpleAccount *account;
  char          *username;
  char          *alias;
} ChattyAccountAddUserData;


static void
chatty_account_free_user_data (ChattyAccountAddUserData *data)
{
  g_free (data->username);
  g_free (data->alias);
  g_free (data);
}


static void
cb_account_added (PurpleAccount *account,
                  gpointer user_data)
{
  // TODO add account to liststore
}


static void
cb_account_removed (PurpleAccount *account,
                    gpointer user_data)
{
  // TODO remove account from liststore
}


static void
cb_account_enabled_disabled (PurpleAccount *account,
                             gpointer user_data)
{
  // TODO update account in liststore
}


static void
cb_dialog_response (GtkDialog *dialog,
                    gint       response_id,
                    gpointer   user_data)
{
  ChattyAccountAddUserData *data = user_data;

  if (response_id == GTK_RESPONSE_OK) {
    PurpleConnection *gc = purple_account_get_connection (data->account);

    if (g_list_find(purple_connections_get_all(), gc)) {
      purple_blist_request_add_buddy (data->account,
                                      data->username,
                                      NULL,
                                      data->alias);
    }

    chatty_account_free_user_data (data);
  } else {
    gtk_widget_destroy (GTK_WIDGET (dialog));
  }
}


static char *
chatty_account_compile_info (PurpleAccount    *account,
                             PurpleConnection *gc,
                             const char       *remote_user,
                             const char       *id,
                             const char       *alias,
                             const char       *msg)
{
  if (msg != NULL && *msg == '\0') {
    msg = NULL;
  }

  return g_strdup_printf("%s%s%s%s has made %s his or her buddy%s%s",
                         remote_user,
                         (alias != NULL ? " ("  : ""),
                         (alias != NULL ? alias : ""),
                         (alias != NULL ? ")"   : ""),
                         (id != NULL
                          ? id
                          : (purple_connection_get_display_name (gc) != NULL
                             ? purple_connection_get_display_name (gc)
                             : purple_account_get_username (account))),
                         (msg != NULL ? ": " : "."),
                         (msg != NULL ? msg  : ""));
}


static void
chatty_account_notify_added (PurpleAccount *account,
                             const char    *remote_user,
                             const char    *id,
                             const char    *alias,
                             const char    *msg)
{
  char             *buffer;
  GtkWidget        *dialog;
  PurpleConnection *gc;

  chatty_data_t *chatty = chatty_get_data();

  gc = purple_account_get_connection(account);

  buffer = chatty_account_compile_info (account,
                                        gc,
                                        remote_user,
                                        id,
                                        alias,
                                        msg);

  dialog = gtk_dialog_new_with_buttons ("Add buddy to your list?",
                                         GTK_WINDOW(chatty->main_window),
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_INFO,
                                         GTK_BUTTONS_OK,
                                         buffer,
                                         NULL);

  gtk_widget_show_all (GTK_WIDGET(dialog));

  g_free (buffer);
}


static void *
chatty_account_request_authorization (PurpleAccount *account,
                                      const char    *remote_user,
                                      const char    *id,
                                      const char    *alias,
                                      const char    *message,
                                      gboolean      on_list,
                                      PurpleAccountRequestAuthorizationCb auth_cb,
                                      PurpleAccountRequestAuthorizationCb deny_cb,
                                      void          *user_data)
{
  printf ("chatty_account_request_authorization\n");

  return NULL;
}


static void
chatty_account_request_close(void *ui_handle)
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
  char                     *buffer;
  GtkWidget                *dialog, *label, *content_area;
  PurpleConnection         *gc;
  ChattyAccountAddUserData *data;

  printf ("chatty_account_request_add\n");

  chatty_data_t *chatty = chatty_get_data();

  gc = purple_account_get_connection (account);

  data = g_new0 (ChattyAccountAddUserData, 1);
  data->account  = account;
  data->username = g_strdup (remote_user);
  data->alias    = g_strdup (alias);

  buffer = chatty_account_compile_info (account,
                                        gc,
                                        remote_user,
                                        id,
                                        alias,
                                        msg);

  dialog = gtk_dialog_new_with_buttons ("Buddy request",
                                         GTK_WINDOW(chatty->main_window),
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         "Add buddy to your list?",
                                         GTK_BUTTONS_YES_NO,
                                         NULL);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  label = gtk_label_new (buffer);

  g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (cb_dialog_response),
                           data);

  gtk_container_add (GTK_CONTAINER (content_area), label);

  gtk_widget_show_all (dialog);

  g_free (buffer);
}


void
chatty_account_connect (const char *account_name,
                        const char *account_pwd)
{
  chatty_data_t *chatty = chatty_get_data();

  gtk_label_set_text (chatty->label_status, "connecting...");

  chatty->account = purple_account_new (account_name, "prpl-jabber");
  purple_account_set_password (chatty->account, account_pwd);

  purple_accounts_add (chatty->account);
  purple_account_set_enabled (chatty->account, UI_ID, TRUE);
}


// TODO even though the struct is properly registered
//      to the libpurple core, it doesn't call
//      the assigned functions
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

void *
chatty_account_get_handle (void) {
  static int handle;

  return &handle;
}

void
chatty_account_init (void)
{
  purple_signal_register (chatty_account_get_handle(), "account-modified",
                          purple_marshal_VOID__POINTER, NULL, 1,
                          purple_value_new (PURPLE_TYPE_SUBTYPE,
                          PURPLE_SUBTYPE_ACCOUNT));

  purple_prefs_add_none (CHATTY_PREFS_ROOT "/accounts");

  purple_signal_connect (purple_accounts_get_handle(),
                         "account-added",
                         chatty_account_get_handle(),
                         PURPLE_CALLBACK (cb_account_added),
                         NULL);

  purple_signal_connect (purple_accounts_get_handle(),
                         "account-removed",
                         chatty_account_get_handle(),
                         PURPLE_CALLBACK (cb_account_removed),
                         NULL);

  purple_signal_connect (purple_accounts_get_handle(),
                         "account-disabled",
                         chatty_account_get_handle(),
                         PURPLE_CALLBACK(cb_account_enabled_disabled),
                         GINT_TO_POINTER(FALSE));

  purple_signal_connect (purple_accounts_get_handle(),
                         "account-enabled",
                         chatty_account_get_handle(),
                         PURPLE_CALLBACK(cb_account_enabled_disabled),
                         GINT_TO_POINTER(TRUE));
}


void
chatty_account_uninit (void)
{
  purple_signals_disconnect_by_handle (chatty_account_get_handle());
  purple_signals_unregister_by_instance (chatty_account_get_handle());
}
