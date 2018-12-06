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

static void chatty_account_create_add_account_view (void);
static void chatty_account_add_account (const char *name, const char *pwd);
static void chatty_account_add_to_accounts_list (PurpleAccount *account,
                                                 guint          list_type);


static void
cb_account_added (PurpleAccount *account,
                  gpointer       user_data)
{
  chatty_account_add_to_accounts_list (account, LIST_ACCOUNT_MANAGE);
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

  account = g_object_get_data (row, "row-account");

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
cb_button_enter_account_data_clicked (GtkButton *sender,
                                      gpointer   data)
{
  chatty_account_create_add_account_view ();
  chatty_window_change_view (CHATTY_VIEW_NEW_ACCOUNT);
}


static void
cb_button_add_account_clicked (GtkButton *sender,
                               gpointer   data)
{
  const gchar *name, *pwd;

  chatty_data_t *chatty = chatty_get_data ();

  chatty_account_data_t *chatty_account = chatty_get_account_data ();

  name = gtk_entry_get_text (GTK_ENTRY(chatty_account->entry_account_name));
  pwd  = gtk_entry_get_text (GTK_ENTRY(chatty_account->entry_account_pwd));

  chatty_account_add_account (name, pwd);

  gtk_container_foreach (GTK_CONTAINER(chatty->pane_view_new_account),
                         (GtkCallback)gtk_widget_destroy, NULL);

  chatty_window_change_view (CHATTY_VIEW_MANAGE_ACCOUNT);
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

  chatty_data_t *chatty = chatty_get_data ();

  account = g_object_get_data (G_OBJECT (row), "row-account");

  if (account) {
    pc = purple_account_get_connection (account);
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

  chatty_blist_create_add_buddy_view (account);

  if (g_strcmp0 (protocol_id, "prpl-mm-sms") == 0) {
    gtk_label_set_text (GTK_LABEL(chatty->label_buddy_id), _("Number"));
  } else {
    gtk_label_set_text (GTK_LABEL(chatty->label_buddy_id), _("XMPP ID"));
  }

  chatty_window_change_view (CHATTY_VIEW_ADD_CONTACT);
}


static void
chatty_account_create_add_account_view (void)
{
  // TODO create this view in a *.ui file for interface builder
  GtkWidget *grid;
  GtkWidget *button_avatar;

  chatty_data_t *chatty = chatty_get_data ();

  chatty_account_data_t *chatty_account = chatty_get_account_data ();

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 20);

  button_avatar = chatty_icon_get_avatar_button (80);

  gtk_grid_attach (GTK_GRID (grid), button_avatar, 1, 1, 1, 1);

  chatty_account->entry_account_name = GTK_ENTRY (gtk_entry_new ());
  gtk_entry_set_placeholder_text (GTK_ENTRY (chatty_account->entry_account_name), _("id@any-server"));
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (chatty_account->entry_account_name), 1, 2, 1, 1);

  g_signal_connect (G_OBJECT(chatty_account->entry_account_name),
                    "insert_text",
                    G_CALLBACK(cb_account_name_insert_text),
                    NULL);

  chatty_account->entry_account_pwd = GTK_ENTRY (gtk_entry_new ());
  gtk_entry_set_placeholder_text (GTK_ENTRY (chatty_account->entry_account_pwd), _("Password"));
  gtk_entry_set_visibility (GTK_ENTRY (chatty_account->entry_account_pwd), 0);
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (chatty_account->entry_account_pwd), 1, 3, 1, 1);

  chatty_account->button_add_account = gtk_button_new_with_label (_("Add account"));
  gtk_widget_set_sensitive (chatty_account->button_add_account, FALSE);
  gtk_grid_attach (GTK_GRID (grid), chatty_account->button_add_account, 1, 4, 1, 1);

  g_signal_connect_object (chatty_account->button_add_account, "clicked",
                           G_CALLBACK (cb_button_add_account_clicked),
                           NULL, 0);

  gtk_widget_set_halign (GTK_WIDGET (grid), GTK_ALIGN_CENTER);
  gtk_widget_set_valign (GTK_WIDGET (grid), GTK_ALIGN_CENTER);

  gtk_widget_show_all (grid);

  gtk_widget_set_can_focus (GTK_WIDGET(chatty_account->entry_account_name), TRUE);
  gtk_widget_grab_focus (GTK_WIDGET(chatty_account->entry_account_name));

  gtk_box_pack_start (GTK_BOX(chatty->pane_view_new_account),
                      grid, TRUE, TRUE, 0);
}


static void
chatty_account_list_separator (GtkListBoxRow *row,
                               GtkListBoxRow *before,
                               gpointer       user_data)
{
  GtkWidget *header;

  if (before == NULL) {
    gtk_list_box_row_set_header (row, NULL);

    return;
  }

  header = gtk_list_box_row_get_header (row);

  if (header != NULL) {
    return;
  }

  header = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_show (header);
  gtk_list_box_row_set_header (row, header);
}


static void
chatty_account_list_clear (GtkWidget *list)
{
  GList  *children;
  GList  *iter;

  children = gtk_container_get_children (GTK_CONTAINER(list));

  for (iter = children; iter != NULL; iter = g_list_next (iter))
    gtk_widget_destroy (GTK_WIDGET(iter->data));

  g_list_free (children);
}


static void
chatty_account_add_to_accounts_list (PurpleAccount *account,
                                     guint          list_type)
{
  GtkBox                   *hbox;
  GtkBox                   *vbox;
  GtkBox                   *vbox_labels;
  GtkWidget                *row;
  GtkWidget                *image;
  GtkWidget                *switch_account_enabled;
  GtkWidget                *label_name;
  GtkWidget                *label_protocol;
  PurplePlugin             *prpl;
  PurplePluginProtocolInfo *prpl_info = NULL;
  gchar                    *markup_str;
  const gchar              *protocol_id;

  chatty_account_data_t *chatty_account = chatty_get_account_data ();

  row = gtk_list_box_row_new ();
  g_object_set_data (G_OBJECT(row),
                     "row-account",
                     (gpointer) account);

  protocol_id = purple_account_get_protocol_id (account);

  // TODO list only SMS and XMPP accounts for the time being
  if ((g_strcmp0 (protocol_id, "prpl-jabber")) != 0 &&
      (g_strcmp0 (protocol_id, "prpl-mm-sms")) != 0) {
    return;
  }

  prpl = purple_find_prpl (purple_account_get_protocol_id (account));

  if (prpl != NULL) {
    prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO (prpl);
  }

  if (prpl_info != NULL && prpl_info->icon_spec.format != NULL) {
    // TODO get protocol icon
  }

  hbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  vbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  vbox_labels = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));

  gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);

  label_name = gtk_label_new (NULL);
  label_protocol = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label_name), 0);
  gtk_label_set_xalign (GTK_LABEL (label_protocol), 0);

  if (list_type == LIST_ACCOUNT_MANAGE) {
    switch_account_enabled = gtk_switch_new ();

    gtk_switch_set_state (GTK_SWITCH(switch_account_enabled),
                          purple_account_get_enabled (account, CHATTY_UI));

    g_signal_connect_object (switch_account_enabled,
                             "state-set",
                             G_CALLBACK(cb_switch_on_off_state_changed),
                             (gpointer) row,
                             0);

    gtk_box_pack_start (vbox, switch_account_enabled, FALSE, FALSE, 12);

    g_object_set (G_OBJECT(row),
                  "selectable",   FALSE,
                  "activatable",  FALSE,
                  NULL);
  } else {
    image = gtk_image_new_from_icon_name ("go-previous-symbolic-rtl",
                                          GTK_ICON_SIZE_BUTTON);

    gtk_box_pack_start (vbox, image, FALSE, FALSE, 12);

    g_object_set (G_OBJECT(row),
                  "selectable",   TRUE,
                  "activatable",  TRUE,
                  NULL);
  }

  gtk_label_set_text (GTK_LABEL(label_name),
                      purple_account_get_username (account));

  markup_str = g_strconcat ("<small><span color='darkgrey'>",
                            purple_account_get_protocol_name (account),
                            "</span></small>",
                            NULL);

  gtk_label_set_markup (GTK_LABEL(label_protocol), markup_str);

  g_free (markup_str);

  gtk_box_pack_start (vbox_labels, label_name, FALSE, FALSE, 4);
  gtk_box_pack_start (vbox_labels, label_protocol, FALSE, FALSE, 4);
  gtk_box_pack_start (hbox, GTK_WIDGET (vbox_labels), FALSE, FALSE, 12);
  gtk_box_pack_end (hbox, GTK_WIDGET (vbox), FALSE, FALSE, 6);

  gtk_container_add (GTK_CONTAINER(row), GTK_WIDGET(hbox));

  if (list_type == LIST_ACCOUNT_MANAGE) {
    gtk_container_add (GTK_CONTAINER(chatty_account->list_manage),
                       GTK_WIDGET(row));
  } else {
    gtk_container_add (GTK_CONTAINER(chatty_account->list_select),
                       GTK_WIDGET(row));
  }

  gtk_widget_show_all (row);
}


static gboolean
chatty_account_populate_account_list (GtkWidget *list)
{
  GList     *l;
  gboolean   ret = FALSE;

  chatty_account_data_t *chatty_account = chatty_get_account_data ();

  chatty_account_list_clear (list);

  for (l = purple_accounts_get_all (); l != NULL; l = l->next) {
    ret = TRUE;

    if (list == chatty_account->list_manage) {
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

  return ret;
}


static void
chatty_account_create_accounts_list (GtkWidget  *list,
                                     GtkBox     *parent,
                                     const char *button_text)
{
  GtkWidget   *scroll;
  GtkWidget   *button;
  GtkBox      *hbox;
  GtkBox      *vbox;
  HdyColumn   *hdy_column;

  gtk_list_box_set_header_func (GTK_LIST_BOX (list),
                                chatty_account_list_separator,
                                NULL,
                                NULL);

  scroll = gtk_scrolled_window_new ( NULL, NULL);
  g_object_set (G_OBJECT(scroll),
                "expand",  TRUE,
                "visible", TRUE,
                NULL);

  gtk_container_add (GTK_CONTAINER(scroll),
                     list);

  vbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  gtk_box_pack_start (vbox, scroll, TRUE, TRUE, 0);

  if (button_text != NULL) {
    hbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));

    button = gtk_button_new_with_label (button_text);
    g_signal_connect_object (button,
                             "clicked",
                             G_CALLBACK (cb_button_enter_account_data_clicked),
                             NULL, 0);

    gtk_box_pack_end (hbox, button, FALSE, FALSE, 0);
    gtk_box_pack_start (vbox, GTK_WIDGET (hbox), FALSE, FALSE, 32);
  }

  hdy_column = hdy_column_new ();
  g_object_set (G_OBJECT(hdy_column),
                "maximum-width",       640,
                "linear-growth-width", 640,
                "margin-top",          32,
                "margin-bottom",       96,
                "margin-start",        16,
                "margin-end",          16,
                "visible",             TRUE,
                NULL);

  gtk_container_add (GTK_CONTAINER(hdy_column),
                     GTK_WIDGET (vbox));

  gtk_container_add (GTK_CONTAINER(parent),
                     GTK_WIDGET(hdy_column));

  gtk_widget_show_all (GTK_WIDGET(hdy_column));

  if (!chatty_account_populate_account_list (list)) {
      // TODO invoke alert dialog saying that there is no account available yet
      // switch to "account-manage-view"
  }
}


void
chatty_account_create_account_select_list (void)
{
  chatty_data_t *chatty = chatty_get_data ();

  chatty_account_data_t *chatty_account = chatty_get_account_data ();

  chatty_account->list_select = gtk_list_box_new ();

  g_signal_connect (chatty_account->list_select,
                    "row-activated",
                    G_CALLBACK(cb_list_account_select_row_activated),
                    NULL);

  chatty_account_create_accounts_list (chatty_account->list_select,
                                       GTK_BOX (chatty->pane_view_select_account),
                                       NULL);
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

  gtk_widget_show_all (GTK_WIDGET(dialog));
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

  chatty_account_data_t *chatty_account = chatty_get_account_data ();

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

  chatty_account->list_manage = gtk_list_box_new ();

  chatty_account_create_account_select_list ();
  chatty_account_create_accounts_list (chatty_account->list_manage,
                                       GTK_BOX (chatty->pane_view_manage_account),
                                       _("Add account"));

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
