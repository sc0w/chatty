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
#include "chatty-dialogs.h"
#include "chatty-settings-dialog.h"
#include "chatty-icons.h"
#include "chatty-window.h"
#include "chatty-utils.h"
#include "chatty-account.h"
#include "chatty-pp-account.h"
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

static void chatty_account_add_to_accounts_list (ChattyPpAccount *account,
                                                 GtkListBox      *list,
                                                 guint            list_type);

static void
cb_list_account_select_row_activated (GtkListBox    *box,
                                      GtkListBoxRow *row,
                                      gpointer       user_data)
{
  ChattyPpAccount *pp_account;
  PurpleAccount *account;
  GtkWidget     *prefix_radio;

  chatty_data_t *chatty = chatty_get_data ();
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  pp_account = g_object_get_data (G_OBJECT (row), "row-account");
  prefix_radio = g_object_get_data (G_OBJECT(row), "row-prefix");
  account = chatty_pp_account_get_account (pp_account);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(prefix_radio), TRUE);

  if (account) {
    if (GPOINTER_TO_INT(user_data) == LIST_SELECT_MUC_ACCOUNT) {
      chatty->selected_account = account;
      return;
    }

    chatty->selected_account = account;
  }

  if (chatty_blist_protocol_is_sms (account)) {
    gtk_widget_hide (GTK_WIDGET(chatty_dialog->grid_edit_contact));
    gtk_widget_hide (GTK_WIDGET(chatty_dialog->button_add_contact));
    gtk_widget_show (GTK_WIDGET(chatty_dialog->button_add_gnome_contact));
  } else {
    gtk_widget_show (GTK_WIDGET(chatty_dialog->grid_edit_contact));
    gtk_widget_show (GTK_WIDGET(chatty_dialog->button_add_contact));
    gtk_widget_hide (GTK_WIDGET(chatty_dialog->button_add_gnome_contact));
  }
}


static void
chatty_account_list_clear (GtkListBox *list)
{
  GList  *children;
  GList  *iter;

  children = gtk_container_get_children (GTK_CONTAINER(list));

  for (iter = children; iter != NULL; iter = g_list_next (iter)) {
    gtk_container_remove (GTK_CONTAINER(list), GTK_WIDGET(iter->data));
  }

  g_list_free (children);
}


static void
chatty_account_add_to_accounts_list (ChattyPpAccount *account,
                                     GtkListBox      *list,
                                     guint            list_type)
{
  HdyActionRow   *row;
  const gchar    *protocol_id;
  GtkWidget      *prefix_radio_button;

  chatty_data_t *chatty = chatty_get_data ();

  row = hdy_action_row_new ();
  g_object_set_data (G_OBJECT(row),
                     "row-account",
                     (gpointer) account);

  protocol_id = chatty_pp_account_get_protocol_id (account);

  // TODO list supported protocols here
  if ((g_strcmp0 (protocol_id, "prpl-jabber")) != 0 &&
      (g_strcmp0 (protocol_id, "prpl-matrix")) != 0 &&
      (g_strcmp0 (protocol_id, "prpl-telegram")) != 0 &&
      (g_strcmp0 (protocol_id, "prpl-delta")) != 0 &&
      (g_strcmp0 (protocol_id, "prpl-threepl")) != 0 &&
      (g_strcmp0 (protocol_id, "prpl-mm-sms")) != 0) {
    return;
  }

  if (chatty_pp_account_get_status (account) == CHATTY_DISCONNECTED) {
    return;
  }

  prefix_radio_button = gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON(chatty->dummy_prefix_radio));
  gtk_widget_show (GTK_WIDGET(prefix_radio_button));

  gtk_widget_set_sensitive (prefix_radio_button, FALSE);

  g_object_set_data (G_OBJECT(row),
                     "row-prefix",
                     (gpointer)prefix_radio_button);

  hdy_action_row_add_prefix (row, GTK_WIDGET(prefix_radio_button ));
  hdy_action_row_set_title (row, chatty_pp_account_get_username (account));

  gtk_container_add (GTK_CONTAINER(list), GTK_WIDGET(row));

  gtk_widget_show (GTK_WIDGET(row));
}


gboolean
chatty_account_populate_account_list (GtkListBox *list, guint type)
{
  GList         *l;
  gboolean       ret = FALSE;
  HdyActionRow  *row;
  const gchar   *protocol_id;

  chatty_account_list_clear (list);

  for (l = purple_accounts_get_all (); l != NULL; l = l->next) {
    ChattyPpAccount *pp_account;
    ret = TRUE;

    pp_account = chatty_pp_account_find (l->data);
    protocol_id = chatty_pp_account_get_protocol_id (pp_account);

    switch (type) {
      case LIST_SELECT_MUC_ACCOUNT:
        if (!(g_strcmp0 (protocol_id, "prpl-mm-sms") == 0)) {
          chatty_account_add_to_accounts_list (pp_account,
                                               list,
                                               LIST_SELECT_MUC_ACCOUNT);
        }

        g_signal_connect (G_OBJECT(list),
                          "row-activated",
                          G_CALLBACK(cb_list_account_select_row_activated),
                          GINT_TO_POINTER(LIST_SELECT_MUC_ACCOUNT));
        break;
      case LIST_SELECT_CHAT_ACCOUNT:
        chatty_account_add_to_accounts_list (pp_account,
                                             list,
                                             LIST_SELECT_CHAT_ACCOUNT);

        g_signal_connect (G_OBJECT(list),
                          "row-activated",
                          G_CALLBACK(cb_list_account_select_row_activated),
                          GINT_TO_POINTER(LIST_SELECT_CHAT_ACCOUNT));
        break;
      default:
        return FALSE;
     }
  }

  if (type == LIST_SELECT_CHAT_ACCOUNT) {
    row = HDY_ACTION_ROW(gtk_list_box_get_row_at_index (list, 0));

    if (row) {
      cb_list_account_select_row_activated (list, GTK_LIST_BOX_ROW(row), NULL);
    }
  }

  return ret;
}


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


void *
chatty_account_get_handle (void) {
  static int handle;

  return &handle;
}


void
chatty_account_init (void)
{
  GList *accounts;

  chatty_data_t *chatty = chatty_get_data ();

  if (chatty->cml_options & CHATTY_CML_OPT_DISABLE) {
    for (accounts = purple_accounts_get_all (); accounts != NULL; accounts = accounts->next) {
      ChattyPpAccount *account;

      account = chatty_pp_account_find (accounts->data);

      if (!account)
        {
          account = chatty_pp_account_new_purple (accounts->data);
          g_list_store_append (chatty->account_list, account);
        }

      chatty_pp_account_set_enabled (account, FALSE);
    }
  } else {
    purple_savedstatus_activate (purple_savedstatus_new (NULL, PURPLE_STATUS_AVAILABLE));
  }
}


void
chatty_account_uninit (void)
{
  purple_signals_disconnect_by_handle (chatty_account_get_handle());
  purple_signals_unregister_by_instance (chatty_account_get_handle());
}
