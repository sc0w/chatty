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
#include "chatty-icons.h"
#include "chatty-window.h"
#include "chatty-account.h"
#include "chatty-purple-init.h"


static chatty_account_data_t chatty_account_data;

chatty_account_data_t *chatty_get_account_data (void)
{
  return &chatty_account_data;
}


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

static void chatty_account_show_add_account_dialog (void);
static void chatty_account_show_edit_account_dialog (PurpleAccount *account);
static void chatty_account_add_account (const char *name, const char *pwd);
static gboolean chatty_account_populate_account_list (GtkListBox *list, guint type);
static void chatty_account_add_to_accounts_list (PurpleAccount *account,
                                                 guint          list_type);


static void
cb_account_added (PurpleAccount *account,
                  gpointer       user_data)
{
  chatty_data_t *chatty = chatty_get_data ();

  chatty_account_populate_account_list (chatty->account_list_manage, LIST_ACCOUNT_MANAGE);
}


static void
cb_account_removed (PurpleAccount *account,
                    gpointer       user_data)
{
  // TODO remove account from liststore
}


static void
cb_account_enabled_disabled (PurpleAccount *account,
                             gpointer       user_data)
{
  // TODO update account in liststore
}


static gboolean
cb_switch_on_off_state_changed (GtkSwitch *widget,
                                gboolean   state,
                                gpointer   row)
{
  PurpleAccount *account;

  account = g_object_get_data (G_OBJECT (row), "row-account");

  gtk_switch_set_state (widget, state);

  purple_account_set_enabled (account, CHATTY_UI, state);

  return TRUE;
}


static void
cb_account_name_insert_text (GtkEntry    *entry,
                             const gchar *text,
                             gint         length,
                             gint        *position,
                             gpointer     data)
{
  chatty_account_data_t *chatty_account = chatty_get_account_data ();

  // TODO validate input
  if (length) {
    gtk_widget_set_sensitive (chatty_account->button_add_account, TRUE);
  } else {
    gtk_widget_set_sensitive (chatty_account->button_add_account, FALSE);
  }
}


static void
cb_list_account_manage_row_activated (GtkListBox    *box,
                                      GtkListBoxRow *row,
                                      gpointer       user_data)
{
  PurpleAccount *account;

  if (g_object_get_data (G_OBJECT (row), "row-new-account")) {
    chatty_account_show_add_account_dialog ();
  } else {
    account = g_object_get_data (G_OBJECT (row), "row-account");
    chatty_account_show_edit_account_dialog (account);
  }
}


static void
cb_list_account_select_row_activated (GtkListBox    *box,
                                      GtkListBoxRow *row,
                                      gpointer       user_data)
{
  PurpleAccount *account;

  PurpleConnection         *pc = NULL;
  PurplePlugin             *prpl = NULL;
  PurplePluginProtocolInfo *prpl_info = NULL;
  const gchar              *protocol_id;
  GtkWidget                *prefix_radio;

  chatty_data_t *chatty = chatty_get_data ();

  account = g_object_get_data (G_OBJECT (row), "row-account");
  prefix_radio = g_object_get_data (G_OBJECT(row), "row-prefix");

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(prefix_radio), TRUE);

  if (account) {
    pc = purple_account_get_connection (account);
    chatty->contact_selected_account = account;
  }

  if (pc) {
    prpl = purple_connection_get_prpl (pc);
  }

  if (prpl) {
    prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO (prpl);
  }

  if (prpl_info && !(prpl_info->options & OPT_PROTO_INVITE_MESSAGE)) {
    // TODO setup UI for invite msg?
  }

  protocol_id = purple_account_get_protocol_id (account);

  if (g_strcmp0 (protocol_id, "prpl-mm-sms") == 0) {
    gtk_label_set_text (GTK_LABEL(chatty->label_contact_id), _("SMS Number"));
  } else {
    gtk_label_set_text (GTK_LABEL(chatty->label_contact_id), _("Jabber ID"));
  }
}


static void
cb_button_delete_account_clicked (GtkButton *sender,
                                  gpointer   data)
{
  GtkWidget     *dialog;
  PurpleAccount *account;
  int            response;

  chatty_data_t *chatty = chatty_get_data ();
  chatty_account_data_t *chatty_account = chatty_get_account_data ();

  account = (PurpleAccount *)data;

  dialog = gtk_message_dialog_new (chatty->main_window,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_OK_CANCEL,
                                   _("Delete Account"));

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG(dialog),
                                            _("Delete account %s?"),
                                            purple_account_get_username (account));

  gtk_dialog_set_default_response (GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
  gtk_window_set_position (GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

  response = gtk_dialog_run (GTK_DIALOG(dialog));

  if (response == GTK_RESPONSE_OK) {
    purple_accounts_delete (account);

    chatty_account_populate_account_list (chatty->account_list_manage,
                                          LIST_ACCOUNT_MANAGE);

    gtk_widget_destroy (chatty_account->dialog_edit_account);
  }

  gtk_widget_destroy (dialog);
}


static void
cb_button_set_account_clicked (GtkButton *sender,
                               gpointer   data)
{
  GtkEntry *entry_account_pwd;

  chatty_account_data_t *chatty_account = chatty_get_account_data ();

  entry_account_pwd = (GtkEntry *)data;

  gtk_widget_set_sensitive (chatty_account->button_set_account, TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET(entry_account_pwd), TRUE);
  gtk_entry_set_text (GTK_ENTRY(entry_account_pwd), "");
  gtk_widget_grab_focus (GTK_WIDGET(entry_account_pwd));
}


static void
chatty_account_show_edit_account_dialog (PurpleAccount *account)
{
  GtkBuilder *builder;
  GtkWidget  *button_delete;
  GtkWidget  *button_edit;
  GtkLabel   *label_name;
  GtkLabel   *label_protocol;
  GtkLabel   *label_status;
  GtkEntry   *entry_account_pwd;
  const char *account_name;
  const char *protocol_name;
  int         response;

  chatty_account_data_t *chatty_account = chatty_get_account_data ();

  builder = gtk_builder_new_from_resource ("/sm/puri/chatty/ui/chatty-edit-account-dialog.ui");

  button_delete = GTK_WIDGET (gtk_builder_get_object (builder, "button_delete_account"));
  button_edit = GTK_WIDGET (gtk_builder_get_object (builder, "button_edit"));
  label_name = GTK_LABEL (gtk_builder_get_object (builder, "label_account_id"));
  label_protocol = GTK_LABEL (gtk_builder_get_object (builder, "label_protocol"));
  label_status = GTK_LABEL (gtk_builder_get_object (builder, "label_status"));
  entry_account_pwd = GTK_ENTRY (gtk_builder_get_object (builder, "entry_account_pwd"));

  chatty_account->button_set_account =
    GTK_WIDGET (gtk_builder_get_object (builder, "ok_button"));

  gtk_entry_set_text (GTK_ENTRY(entry_account_pwd), "xxxxxxxxx");

  g_signal_connect (G_OBJECT(button_delete),
                    "clicked",
                    G_CALLBACK (cb_button_delete_account_clicked),
                    (gpointer)account);

  g_signal_connect (G_OBJECT(button_edit),
                    "clicked",
                    G_CALLBACK (cb_button_set_account_clicked),
                    (gpointer)entry_account_pwd);

  account_name = purple_account_get_username (account);
  protocol_name = purple_account_get_protocol_name (account);

  gtk_label_set_text (label_name, account_name);
  gtk_label_set_text (label_protocol, protocol_name);

  if (purple_account_is_connected (account)) {
    gtk_label_set_text (label_status, _("connected"));
  } else if (purple_account_is_connecting (account)) {
    gtk_label_set_text (label_status, _("connecting..."));
  } else if (purple_account_is_disconnected (account)) {
    gtk_label_set_text (label_status, _("disconnected"));
  }

  chatty_account->dialog_edit_account = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));

  response = gtk_dialog_run (GTK_DIALOG(chatty_account->dialog_edit_account));

  if (response == GTK_RESPONSE_OK) {
    const gchar *pwd;

    pwd = gtk_entry_get_text (GTK_ENTRY(entry_account_pwd));

    purple_account_set_password (account, pwd);
    purple_account_set_remember_password (account, TRUE);

    if (purple_account_is_connected (account)) {
      purple_account_disconnect (account);
    }

    if (purple_account_is_disconnected (account)) {
      purple_account_connect (account);
    }
  }

  g_signal_handlers_disconnect_by_func (G_OBJECT(button_delete),
                                        G_CALLBACK (cb_button_delete_account_clicked),
                                        (gpointer)account);

  g_signal_handlers_disconnect_by_func (G_OBJECT(button_edit),
                                        G_CALLBACK (cb_button_set_account_clicked),
                                        (gpointer)entry_account_pwd);

  gtk_widget_destroy (chatty_account->dialog_edit_account);

  g_object_unref (builder);
}


static void
chatty_account_show_add_account_dialog (void)
{
  GtkBuilder *builder;
  GtkWidget  *dialog;
  int         response;

  chatty_account_data_t *chatty_account = chatty_get_account_data ();

  builder = gtk_builder_new_from_resource ("/sm/puri/chatty/ui/chatty-add-account-dialog.ui");

  chatty_account->button_add_account =
    GTK_WIDGET (gtk_builder_get_object (builder, "ok_button"));

  chatty_account->entry_account_name =
    GTK_ENTRY (gtk_builder_get_object (builder, "entry_account_id"));

  chatty_account->entry_account_pwd =
    GTK_ENTRY (gtk_builder_get_object (builder, "entry_account_pwd"));

  g_signal_connect (G_OBJECT(chatty_account->entry_account_name),
                    "insert_text",
                    G_CALLBACK(cb_account_name_insert_text),
                    NULL);

  dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));

  gtk_window_set_position (GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

  response = gtk_dialog_run (GTK_DIALOG(dialog));

  if (response == GTK_RESPONSE_OK) {
    const gchar *name, *pwd;

    name = gtk_entry_get_text (GTK_ENTRY(chatty_account->entry_account_name));
    pwd  = gtk_entry_get_text (GTK_ENTRY(chatty_account->entry_account_pwd));

    chatty_account_add_account (name, pwd);
  }

  g_signal_handlers_disconnect_by_func (G_OBJECT(chatty_account->entry_account_name),
                                        G_CALLBACK(cb_account_name_insert_text),
                                        NULL);

  gtk_widget_destroy (dialog);

  g_object_unref (builder);
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
chatty_account_add_to_accounts_list (PurpleAccount *account,
                                     guint          list_type)
{
  HdyActionRow   *row;
  GtkWidget      *switch_account_enabled;
  const gchar    *protocol_id;
  GtkWidget      *prefix_radio_button;

  chatty_data_t *chatty = chatty_get_data ();

  row = hdy_action_row_new ();
  g_object_set_data (G_OBJECT(row),
                     "row-account",
                     (gpointer) account);

  protocol_id = purple_account_get_protocol_id (account);

  // TODO list only SMS and XMPP accounts for the time being
  if ((g_strcmp0 (protocol_id, "prpl-jabber")) != 0 &&
      (g_strcmp0 (protocol_id, "prpl-mm-sms")) != 0) {
    return;
  }

  if (list_type == LIST_ACCOUNT_MANAGE) {
    switch_account_enabled = gtk_switch_new ();
    gtk_widget_show (GTK_WIDGET(switch_account_enabled));

    g_object_set  (G_OBJECT(switch_account_enabled),
                   "valign", GTK_ALIGN_CENTER,
                   "halign", GTK_ALIGN_END,
                   NULL);

    gtk_switch_set_state (GTK_SWITCH(switch_account_enabled),
                          purple_account_get_enabled (account, CHATTY_UI));

    g_signal_connect_object (switch_account_enabled,
                             "state-set",
                             G_CALLBACK(cb_switch_on_off_state_changed),
                             (gpointer) row,
                             0);

    hdy_action_row_set_title (row, purple_account_get_username (account));
    hdy_action_row_set_subtitle (row, purple_account_get_protocol_name (account));
    hdy_action_row_add_action (row, GTK_WIDGET(switch_account_enabled));
  } else {
    prefix_radio_button = gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON(chatty->dummy_prefix_radio));
    gtk_widget_show (GTK_WIDGET(prefix_radio_button));

    gtk_widget_set_sensitive (prefix_radio_button, FALSE);

    g_object_set_data (G_OBJECT(row),
                       "row-prefix",
                       (gpointer)prefix_radio_button);

    hdy_action_row_add_prefix (row, GTK_WIDGET(prefix_radio_button ));
    hdy_action_row_set_title (row, purple_account_get_username (account));
  }

  if (list_type == LIST_ACCOUNT_MANAGE) {
    gtk_container_add (GTK_CONTAINER(chatty->account_list_manage),
                       GTK_WIDGET(row));
  } else {
    gtk_container_add (GTK_CONTAINER(chatty->account_list_select),
                       GTK_WIDGET(row));
  }

  gtk_widget_show (GTK_WIDGET(row));
}


static gboolean
chatty_account_populate_account_list (GtkListBox *list, guint type)
{
  GList         *l;
  gboolean       ret = FALSE;
  HdyActionRow  *row;

  chatty_data_t *chatty = chatty_get_data ();

  chatty_account_list_clear (list);

  for (l = purple_accounts_get_all (); l != NULL; l = l->next) {
    ret = TRUE;

    if (list == chatty->account_list_manage) {
      const gchar *protocol_id;

      protocol_id = purple_account_get_protocol_id ((PurpleAccount *)l->data);

      if (!(g_strcmp0 (protocol_id, "prpl-mm-sms") == 0)) {
        chatty_account_add_to_accounts_list ((PurpleAccount *)l->data,
                                             LIST_ACCOUNT_MANAGE);
      }
    } else {
      chatty_account_add_to_accounts_list ((PurpleAccount *)l->data,
                                           LIST_ACCOUNT_SELECT);
    }
  }

  if (type == LIST_ACCOUNT_MANAGE) {
    row = hdy_action_row_new ();

    g_object_set_data (G_OBJECT(row),
                       "row-new-account",
                       (gpointer)TRUE);

    hdy_action_row_set_title (row, _("Add new account..."));

    gtk_container_add (GTK_CONTAINER(chatty->account_list_manage),
                       GTK_WIDGET(row));

    gtk_widget_show (GTK_WIDGET(row));
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

  chatty_data_t *chatty = chatty_get_data ();

  dialog = gtk_message_dialog_new (chatty->main_window,
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
  struct auth_request *ar;
  int                  response;

  chatty_data_t *chatty = chatty_get_data ();

  dialog = gtk_message_dialog_new (chatty->main_window,
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


void
chatty_account_add_sms_account (void)
{
  PurpleAccount *account;

  account = purple_account_new ("SMS", "prpl-mm-sms");

  purple_account_set_password (account, NULL);
  purple_account_set_remember_password (account, TRUE);

  purple_accounts_add (account);
}


static void
chatty_account_add_account (const char *account_name,
                            const char *account_pwd)
{
  PurpleAccount *account;

  account = purple_account_new (account_name, "prpl-jabber");

  purple_account_set_password (account, account_pwd);
  // TODO make remember_password choosable when the
  //      account-settings view is introduced
  purple_account_set_remember_password (account, TRUE);

  purple_accounts_add (account);
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
  chatty_data_t *chatty = chatty_get_data ();

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

  if (!purple_prefs_get_bool ("/purple/savedstatus/startup_current_status")) {
    purple_savedstatus_activate (purple_savedstatus_get_startup ());
  }

  g_signal_connect (G_OBJECT(chatty->account_list_manage),
                    "row-activated",
                    G_CALLBACK(cb_list_account_manage_row_activated),
                    NULL);

  g_signal_connect (G_OBJECT(chatty->account_list_select),
                    "row-activated",
                    G_CALLBACK(cb_list_account_select_row_activated),
                    NULL);

  chatty_account_populate_account_list (chatty->account_list_manage,
                                        LIST_ACCOUNT_MANAGE);

  chatty_account_populate_account_list (chatty->account_list_select,
                                        LIST_ACCOUNT_SELECT);

  if (!purple_prefs_get_bool ("/purple/savedstatus/startup_current_status")) {
    purple_savedstatus_activate (purple_savedstatus_get_startup ());
  }
}


void
chatty_account_uninit (void)
{
  purple_signals_disconnect_by_handle (chatty_account_get_handle());
  purple_signals_unregister_by_instance (chatty_account_get_handle());
}
