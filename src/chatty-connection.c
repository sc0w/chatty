/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */


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
  // TODO initialize stuff, wait animation...
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


static void
chatty_connection_network_connected (void)
{

}


static void
chatty_connection_network_disconnected (void)
{
  chatty_data_t *chatty = chatty_get_data();

  gtk_label_set_text (chatty->label_status, "Disconnected");
}


static void
chatty_connection_report_disconnect_reason (PurpleConnection *gc,
                                            PurpleConnectionError reason,
                                            const char *text)
{
  gchar *label_text;

  chatty_data_t *chatty = chatty_get_data();

  PurpleAccount *account = purple_connection_get_account (gc);

  label_text = g_strdup_printf ("Disconnected: \"%s\" (%s)\n  >Error: %d\n  >Reason: %s\n",
                                purple_account_get_username(account),
                                purple_account_get_protocol_id(account),
                                reason, text);

  gtk_label_set_text (chatty->label_status, label_text);

  g_free (label_text);
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
