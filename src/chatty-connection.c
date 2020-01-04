/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-connection"

#include <glib.h>
#include <glib/gi18n.h>
#include "purple.h"
#include "chatty-window.h"
#include "chatty-connection.h"
#include "chatty-buddy-list.h"
#include "chatty-purple-init.h"
#include "chatty-notify.h"
#include "chatty-dialogs.h"
#include "chatty-folks.h"


#define INITIAL_RECON_DELAY_MIN  5
#define INITIAL_RECON_DELAY_MAX  30
#define MAX_RECON_DELAY          300


static GHashTable *auto_reconns = NULL;

static gboolean sms_notifications = FALSE;

static gboolean chatty_connection_sign_on (gpointer data);
static void chatty_connection_account_spinner (PurpleAccount *account, gboolean spin);


static void
cb_sms_modem_added (int status)
{
  PurpleAccount   *account;
  ChattyAutoRecon *info;

  account = purple_accounts_find ("SMS", "prpl-mm-sms");

  if (purple_account_is_disconnected (account)) {
    info = g_hash_table_lookup (auto_reconns, account);

    if (info == NULL) {
      info = g_new0 (ChattyAutoRecon, 1);

      g_hash_table_insert (auto_reconns, account, info);
    }

    info->delay = g_random_int_range (INITIAL_RECON_DELAY_MIN,
                                      INITIAL_RECON_DELAY_MAX);

    info->timeout = g_timeout_add_seconds (info->delay,
                                           chatty_connection_sign_on,
                                           account);
  }

  g_debug ("%s modem state: %i", __func__, status);
}


static void
cb_account_removed (PurpleAccount *account,
                    gpointer       user_data)
{
  g_hash_table_remove (auto_reconns, account);
}


static void
cb_account_disabled (PurpleAccount *account,
                     gpointer       user_data)
{
  if (account) {
    chatty_connection_account_spinner (account, FALSE);
  }
}


static void
chatty_connection_update_ui (void)
{
  GList *accounts;

  chatty_data_t *chatty = chatty_get_data ();

  chatty->im_account_connected = FALSE;
  chatty->sms_account_connected = FALSE;

  gtk_widget_set_sensitive (chatty->button_menu_new_group_chat, FALSE);

  for (accounts = purple_accounts_get_all (); accounts != NULL; accounts = accounts->next) {
    PurpleAccount *account = accounts->data;

    if (purple_account_is_connected (account)) {
      if (chatty_blist_protocol_is_sms (account)) {
        chatty->sms_account_connected = TRUE;
      }  else {
        chatty->im_account_connected = TRUE;
        gtk_widget_set_sensitive (chatty->button_menu_new_group_chat, TRUE);
      } 
    }
  }

  gtk_widget_set_sensitive (chatty->button_header_add_chat, 
                            chatty->im_account_connected |
                            chatty->sms_account_connected);

  chatty_window_overlay_show (!chatty_blist_list_has_children (CHATTY_LIST_CHATS));
}


static void
chatty_connection_account_spinner (PurpleAccount *account,
                                   gboolean       spin)
{
  GList         *children;
  GList         *iter;
  GtkWidget     *spinner;
  HdyActionRow  *row;

  chatty_data_t *chatty = chatty_get_data ();

  children = gtk_container_get_children (GTK_CONTAINER(chatty->list_manage_account));

  for (iter = children; iter != NULL; iter = g_list_next (iter)) {
    row = HDY_ACTION_ROW(iter->data);

    if (g_object_get_data (G_OBJECT (row), "row-account") == account) {
      spinner = g_object_get_data (G_OBJECT(row), "row-prefix");

      if (spinner && spin) {
        gtk_spinner_start (GTK_SPINNER(spinner));
      } else if (spinner && !spin) {
        gtk_spinner_stop (GTK_SPINNER(spinner));
      }

      break;
    }
  }

  g_list_free (children);
}


static void
chatty_connection_connect_progress (PurpleConnection *gc,
                                    const char       *text,
                                    size_t            step,
                                    size_t            step_count)
{
  PurpleAccount *account;

  account = purple_connection_get_account (gc);

  if ((int)step == 2) {
    chatty_connection_account_spinner (account, TRUE);
  }
}


static void
chatty_connection_error_dialog (PurpleAccount *account,
                                const gchar   *error)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (NULL,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_OK,
                                   _("Login failed"));

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG(dialog),
                                            "%s: %s\n\n%s",
                                            error,
                                            purple_account_get_username (account),
                                            _("Please check ID and password"));

  gtk_dialog_set_default_response (GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
  gtk_window_set_position (GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

  gtk_dialog_run (GTK_DIALOG(dialog));

  gtk_widget_destroy (dialog);
}


static void
chatty_connection_connected (PurpleConnection *gc)
{
  PurpleAccount    *account;
  g_autofree gchar *message = NULL;
  const char       *user_name;
  const char       *protocol_id;
  int               sms_match;

  chatty_data_t *chatty = chatty_get_data ();

  account = purple_connection_get_account (gc);
  user_name = purple_account_get_username (account),
  protocol_id = purple_account_get_protocol_id (account);

  sms_match = !g_strcmp0 (protocol_id, "prpl-mm-sms");

  chatty_connection_account_spinner (account, FALSE);

  if (g_hash_table_contains (auto_reconns, account)) {
    message = g_strdup_printf ("Account %s reconnected", user_name);

    if (!sms_match || (sms_match && sms_notifications)) {
      chatty_notify_show_notification (NULL, message, CHATTY_NOTIFY_ACCOUNT_CONNECTED, NULL, NULL);
    }
  }

  if (sms_match) {
    sms_notifications = TRUE;

    chatty_blist_enable_folks_contacts ();

    // we are ready to open URI links now
    if (chatty->uri) {
      chatty_blist_add_buddy_from_uri (chatty->uri);
      chatty->uri = NULL;
    }
  }

  chatty_connection_update_ui ();

  g_hash_table_remove (auto_reconns, account);

  g_debug ("%s account: %s", __func__, purple_account_get_username (account));
}


static void
chatty_connection_disconnected (PurpleConnection *gc)
{
  chatty_connection_update_ui ();

  g_debug ("chatty_connection_disconnected");
}


static gboolean
chatty_connection_sign_on (gpointer data)
{
  PurpleAccount   *account = data;
  ChattyAutoRecon *info;
  PurpleStatus    *status;

  g_return_val_if_fail (account != NULL, FALSE);

  info = g_hash_table_lookup (auto_reconns, account);

  if (info) {
    info->timeout = 0;
  }

  status = purple_account_get_active_status (account);

  if (purple_status_is_online (status)) {
    g_debug ("%s: Connect %s", __func__, purple_account_get_username (account));
    purple_account_connect (account);
  }

  return FALSE;
}


static void
chatty_connection_network_connected (void)
{
  GList *list, *l;

  l = list = purple_accounts_get_all_active ();

  g_debug ("%s", __func__);

  while (l) {
    PurpleAccount *account = (PurpleAccount*)l->data;
    g_hash_table_remove (auto_reconns, account);

    if (purple_account_is_disconnected (account)) {
      chatty_connection_sign_on (account);
    }

    l = l->next;
  }

  g_list_free (list);
}


static void
chatty_connection_network_disconnected (void)
{
  GList *list, *l;

  l = list = purple_accounts_get_all_active();

  g_debug ("%s", __func__);

  while (l) {
    PurpleAccount *a = (PurpleAccount*)l->data;

    if (!chatty_blist_protocol_is_sms (a)) {
      if (!purple_account_is_disconnected (a)) {
        char *password = g_strdup(purple_account_get_password (a));

        purple_account_disconnect (a);
        purple_account_set_password (a, password);

        g_free (password);
      }
    }

    l = l->next;
  }

  g_list_free(list);
}


static void
chatty_connection_report_disconnect_reason (PurpleConnection     *gc,
                                            PurpleConnectionError reason,
                                            const char           *text)
{
  PurpleAccount    *account;
  GNetworkMonitor  *monitor;
  ChattyAutoRecon  *info;
  g_autofree gchar *message = NULL;
  const char       *user_name;
  const char       *protocol_id;

  account = purple_connection_get_account (gc);
  user_name = purple_account_get_username (account);
  protocol_id = purple_account_get_protocol_id (account);

  g_debug ("Disconnected: \"%s\" (%s)\nError: %d\nReason: %s",
           user_name, protocol_id, reason, text);

  if (!g_strcmp0 (protocol_id, "prpl-mm-sms")) {
    // the SMS account gets included in auto_reconns
    // in the 'cb_sms_modem_added' callback
    if (sms_notifications) {
      message = g_strdup_printf ("SMS disconnected: %s", text);
      chatty_notify_show_notification (user_name, message, CHATTY_NOTIFY_ACCOUNT_GENERIC, NULL, NULL);
    }

    return;
  }

  chatty_connection_account_spinner (account, TRUE);

  info = g_hash_table_lookup (auto_reconns, account);

  if (!purple_connection_error_is_fatal (reason)) {
    if (info == NULL) {
      info = g_new0 (ChattyAutoRecon, 1);

      g_hash_table_insert (auto_reconns, account, info);

      info->delay = g_random_int_range (INITIAL_RECON_DELAY_MIN,
                                        INITIAL_RECON_DELAY_MAX);
    } else {
      info->delay = MIN(2 * info->delay, MAX_RECON_DELAY);

      if (info->timeout != 0) {
        g_source_remove(info->timeout);
      }
    }

    info->timeout = g_timeout_add_seconds (info->delay,
                                           chatty_connection_sign_on,
                                           account);
  } else {
    if (info != NULL) {
      g_hash_table_remove (auto_reconns, account);
    }

    chatty_connection_error_dialog (account, text);

    purple_account_set_enabled (account, CHATTY_UI, FALSE);
  }

  monitor = g_network_monitor_get_default ();

  if (g_network_monitor_get_connectivity (monitor) == G_NETWORK_CONNECTIVITY_FULL) {
    message = g_strdup_printf ("Account %s disconnected: %s", user_name, text);
    chatty_notify_show_notification (NULL, message, CHATTY_NOTIFY_ACCOUNT_DISCONNECTED, NULL, NULL);
  }
}


static void
chatty_connection_info (PurpleConnection *gc,
                        const char       *text)
{
  g_debug ("%s: %s", __func__, text);
}


static PurpleConnectionUiOps connection_ui_ops =
{
  chatty_connection_connect_progress,
  chatty_connection_connected,
  chatty_connection_disconnected,
  chatty_connection_info,
  NULL,
  chatty_connection_network_connected,
  chatty_connection_network_disconnected,
  chatty_connection_report_disconnect_reason,
  NULL,
  NULL,
  NULL
};


PurpleConnectionUiOps *
chatty_connection_get_ui_ops (void)
{
  return &connection_ui_ops;
}


void *
chatty_connection_get_handle (void)
{
  static int handle;

  return &handle;
}


static void
chatty_connection_free_auto_recon (gpointer data)
{
  ChattyAutoRecon *info = data;

  if (info->timeout != 0) {
    g_source_remove (info->timeout);
  }

  g_free (info);
}


void
chatty_connection_init (void)
{
  auto_reconns = g_hash_table_new_full (g_direct_hash,
                                        g_direct_equal,
                                        NULL,
                                        chatty_connection_free_auto_recon);

  purple_signal_connect (purple_plugins_get_handle (),
                         "mm-sms-modem-added",
                         chatty_connection_get_handle (),
                         PURPLE_CALLBACK (cb_sms_modem_added), NULL);

  purple_signal_connect (purple_accounts_get_handle(),
                         "account-removed",
                         chatty_connection_get_handle(),
                         PURPLE_CALLBACK (cb_account_removed), NULL);

  purple_signal_connect (purple_accounts_get_handle(),
                         "account-disabled",
                         chatty_connection_get_handle(),
                         PURPLE_CALLBACK (cb_account_disabled), NULL);
}


void
chatty_connection_uninit (void)
{
  purple_signals_disconnect_by_handle (chatty_connection_get_handle());

  g_hash_table_destroy (auto_reconns);
}
