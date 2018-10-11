/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-connection"

#include "purple.h"
#include "chatty-window.h"
#include "chatty-connection.h"


static GHashTable *auto_reconns = NULL;


static void
cb_account_removed (PurpleAccount *account,
                    gpointer user_data)
{
  g_hash_table_remove (auto_reconns, account);
}


static void
chatty_connection_connect_progress (PurpleConnection *gc,
                                    const char       *text,
                                    size_t            step,
                                    size_t            step_count)
{
  // TODO maybe welcome screen with animation?
}


static void
chatty_connection_connected (PurpleConnection *gc)
{
  PurpleAccount *account;

  account  = purple_connection_get_account (gc);

  g_hash_table_remove(auto_reconns, account);
}


static void
chatty_connection_disconnected (PurpleConnection *gc)
{

}


static gboolean
chatty_connection_sign_on (gpointer data)
{
  PurpleAccount *account = data;
  ChattyAutoRecon *info;
  PurpleStatus *status;

  g_return_val_if_fail (account != NULL, FALSE);
  info = g_hash_table_lookup (auto_reconns, account);

  if (info) {
    info->timeout = 0;
  }

  status = purple_account_get_active_status (account);
  g_debug ("About to get all accounts");
  if (purple_status_is_online (status))
  {
    g_debug ("get all accounts");
    purple_account_connect(account);
  }

  return FALSE;
}


static void
chatty_connection_network_connected (void)
{
  GList *list, *l;

  l = list = purple_accounts_get_all_active ();

  while (l) {
    PurpleAccount *account = (PurpleAccount*)l->data;
    g_hash_table_remove (auto_reconns, account);

    if (purple_account_is_disconnected (account)) {
      chatty_connection_sign_on (account);
    }

    l = l->next;
  }

  g_list_free(list);
}


static void
chatty_connection_network_disconnected (void)
{
  GList *list, *l;

  l = list = purple_accounts_get_all_active();
  while (l) {
    PurpleAccount *a = (PurpleAccount*)l->data;

    if (!purple_account_is_disconnected (a)) {
      char *password = g_strdup(purple_account_get_password (a));
      purple_account_disconnect (a);
      purple_account_set_password (a, password);
      g_free(password);
    }

    l = l->next;
  }

  g_list_free(list);
}


static void
chatty_connection_report_disconnect_reason (PurpleConnection *gc,
                                            PurpleConnectionError reason,
                                            const char *text)
{
  PurpleAccount *account = purple_connection_get_account (gc);

  g_debug ("Disconnected: \"%s\" (%s)\n  >Error: %d\n  >Reason: %s",
                                purple_account_get_username(account),
                                purple_account_get_protocol_id(account),
                                reason, text);

}


static PurpleConnectionUiOps connection_ui_ops =
{
  chatty_connection_connect_progress,
  chatty_connection_connected,
  chatty_connection_disconnected,
  NULL,
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

  purple_signal_connect (purple_accounts_get_handle(),
                         "account-removed",
                         chatty_connection_get_handle(),
                         PURPLE_CALLBACK (cb_account_removed), NULL);
}


void
chatty_connection_uninit (void)
{
  purple_signals_disconnect_by_handle (chatty_connection_get_handle());

  g_hash_table_destroy (auto_reconns);
}
