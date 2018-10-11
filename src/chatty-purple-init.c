/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-purple"

#include "purple.h"
#include "chatty-purple-init.h"
#include <glib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "chatty-window.h"
#include "chatty-config.h"
#include "chatty-icons.h"
#include "chatty-account.h"
#include "chatty-message-list.h"
#include "chatty-buddy-list.h"
#include "chatty-connection.h"
#include "chatty-conversation.h"


static chatty_purple_data_t chatty_purple_data;

chatty_purple_data_t *chatty_get_purple_data (void)
{
  return &chatty_purple_data;
}


typedef struct _PurpleGLibIOClosure
{
  PurpleInputFunction function;
  guint               result;
  gpointer            data;
} PurpleGLibIOClosure;


static void
purple_glib_io_destroy (gpointer data)
{
  g_free (data);
}


static gboolean
purple_glib_io_invoke (GIOChannel   *source,
                       GIOCondition  condition,
                       gpointer      data)
{
  PurpleGLibIOClosure *closure = data;
  PurpleInputCondition purple_cond = 0;

  if (condition & PURPLE_GLIB_READ_COND) {
    purple_cond |= PURPLE_INPUT_READ;
  }

  if (condition & PURPLE_GLIB_WRITE_COND) {
    purple_cond |= PURPLE_INPUT_WRITE;
  }

  closure->function (closure->data, g_io_channel_unix_get_fd (source),
                     purple_cond);

  return TRUE;
}


static guint
glib_input_add (gint                 fd,
                PurpleInputCondition condition,
                PurpleInputFunction  function,
                gpointer             data)
{

  PurpleGLibIOClosure *closure;
  GIOChannel          *channel;
  GIOCondition         cond = 0;

  closure = g_new0 (PurpleGLibIOClosure, 1);

  closure->function = function;
  closure->data = data;

  if (condition & PURPLE_INPUT_READ) {
    cond |= PURPLE_GLIB_READ_COND;
  }

  if (condition & PURPLE_INPUT_WRITE) {
    cond |= PURPLE_GLIB_WRITE_COND;
  }

  channel = g_io_channel_unix_new (fd);

  closure->result = g_io_add_watch_full (channel,
                                         G_PRIORITY_DEFAULT,
                                         cond,
                                         purple_glib_io_invoke,
                                         closure,
                                         purple_glib_io_destroy);

  g_io_channel_unref (channel);
  return closure->result;
}


static
PurpleEventLoopUiOps glib_eventloops =
{
  g_timeout_add,
  g_source_remove,
  glib_input_add,
  g_source_remove,
  NULL,
#if GLIB_CHECK_VERSION(2,14,0)
  g_timeout_add_seconds,
#else
  NULL,
#endif
  NULL,
  NULL,
  NULL
};


static void
chatty_quit (void)
{
  chatty_conversations_uninit ();
  purple_conversations_set_ui_ops (NULL);

  chatty_blist_uninit ();
  purple_blist_set_ui_ops (NULL);

  chatty_connection_uninit();
  purple_connections_set_ui_ops (NULL);

  chatty_account_uninit();
  purple_accounts_set_ui_ops (NULL);

  gtk_main_quit();
}


static void
chatty_purple_ui_init (void)
{
  chatty_account_init ();
  purple_accounts_set_ui_ops (chatty_accounts_get_ui_ops ());

  chatty_connection_init ();
  purple_connections_set_ui_ops (chatty_connection_get_ui_ops ());

  chatty_blist_init ();
  purple_blist_set_ui_ops (chatty_blist_get_ui_ops ());

  chatty_conversations_init ();
  purple_conversations_set_ui_ops (chatty_conversations_get_conv_ui_ops ());
}


static void
chatty_purple_prefs_init (void)
{
  purple_prefs_add_none (CHATTY_PREFS_ROOT "");
  purple_prefs_add_none ("/plugins/chatty");

  purple_prefs_add_none (CHATTY_PREFS_ROOT "/plugins");
  purple_prefs_add_path_list (CHATTY_PREFS_ROOT "/plugins/loaded", NULL);

  purple_prefs_add_none (CHATTY_PREFS_ROOT "/filelocations");
  purple_prefs_add_path (CHATTY_PREFS_ROOT "/filelocations/last_save_folder", "");
  purple_prefs_add_path (CHATTY_PREFS_ROOT "/filelocations/last_open_folder", "");
  purple_prefs_add_path (CHATTY_PREFS_ROOT "/filelocations/last_icon_folder", "");
}


static
PurpleCoreUiOps core_uiops =
{
  chatty_purple_prefs_init,
  NULL,
  chatty_purple_ui_init,
  chatty_quit,
  NULL,
  NULL,
  NULL,
  NULL
};


static void
init_libpurple (void)
{
  gchar *search_path;
  GList *iter;
  GList *names = NULL;

  purple_debug_set_enabled (FALSE);
  purple_debug_set_verbose (FALSE);

  purple_core_set_ui_ops (&core_uiops);
  purple_eventloop_set_ui_ops (&glib_eventloops);

  search_path = g_build_filename (purple_user_dir (), "plugins", NULL);
  purple_plugins_add_search_path (search_path);
  g_free (search_path);

  if (!purple_core_init (CHATTY_UI)) {
    g_error ("libpurple initialization failed");
  }

  purple_set_blist (purple_blist_new ());
  purple_prefs_load ();
  purple_blist_load ();
  purple_plugins_load_saved (CHATTY_PREFS_ROOT "/plugins/loaded");

  purple_plugins_probe (G_MODULE_SUFFIX);
  iter = purple_plugins_get_all ();

  for (int i = 0; iter; iter = iter->next) {
    PurplePlugin *plugin = iter->data;
    PurplePluginInfo *info = plugin->info;

    // TODO maybe we can simply load all plugins that will finally be
    //      packed into ./purple/plugins on the Librem5 ?
    //      Alternatively we can compile a list with plugin ids
    //      that we have approved for chatty
    if (g_strcmp0 (info->id, "core-mancho-omemo") == 0) {
      if (!purple_plugin_is_loaded (plugin)) {
        purple_plugin_load (plugin);
        g_debug("Loaded plugin %s", info->name);
      }

      purple_plugins_save_loaded (CHATTY_PREFS_ROOT "/plugins/loaded");
    }

    // if (info && info->name) {
    //   printf("\t%d: %s\n", i++, info->name);
    //   names = g_list_append (names, info->id);
    // }
  }

  g_list_free (names);

  purple_plugins_init ();
  purple_pounces_load ();
  purple_blist_show ();
}


static void
signed_on (PurpleConnection *gc)
{
  gchar *text;

  chatty_data_t *chatty = chatty_get_data ();
  chatty_purple_data_t *chatty_purple = chatty_get_purple_data();

  PurpleAccount *account = purple_connection_get_account (gc);
  chatty_purple->account = account;

  text = g_strdup_printf ("Connected: %s %s",
                          purple_account_get_username (account),
                          purple_account_get_protocol_id (account));

  g_debug ("%s", text);

  g_free (text);
}


static int
account_authorization_requested (PurpleAccount *account,
                                 const char    *user)
{
  g_debug ("User \"%s\" (%s) has sent a buddy request",
           user,
           purple_account_get_protocol_id (account));

  return 1;
}


static void
connect_to_signals (void)
{
  static int handle;

  purple_signal_connect (purple_connections_get_handle (),
                         "signed-on",
                         &handle,
                         PURPLE_CALLBACK (signed_on),
                         NULL);

  purple_signal_connect (purple_accounts_get_handle (),
                         "account-authorization-requested",
                         &handle,
                         PURPLE_CALLBACK (account_authorization_requested),
                         NULL);
}


void
libpurple_start (void) {
  signal (SIGCHLD, SIG_IGN);

  init_libpurple ();

  g_debug ("libpurple initialized. Running version %s.",
           purple_core_get_version ());

  connect_to_signals ();
}
