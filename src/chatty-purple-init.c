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
#include "./xeps/chatty-xep-0184.h"
#include "chatty-message-list.h"
#include "chatty-buddy-list.h"
#include "chatty-connection.h"
#include "chatty-conversation.h"

static GHashTable *ui_info = NULL;

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
PurpleEventLoopUiOps eventloop_ops =
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


static PurpleEventLoopUiOps *
chatty_eventloop_get_ui_ops (void)
{
  return &eventloop_ops;
}


static void
chatty_purple_quit (void)
{
  chatty_conversations_uninit ();
  chatty_blist_uninit ();
  chatty_connection_uninit();
  chatty_account_uninit();

  purple_conversations_set_ui_ops (NULL);
  purple_connections_set_ui_ops (NULL);
  purple_blist_set_ui_ops (NULL);
  purple_accounts_set_ui_ops (NULL);

  if (NULL != ui_info) {
    g_hash_table_destroy (ui_info);
  }

  chatty_xeps_close ();

  g_application_quit (g_application_get_default ());
}


static void
chatty_purple_ui_init (void)
{
  chatty_account_init ();
  chatty_connection_init ();
  chatty_blist_init ();
  chatty_conversations_init ();

  purple_accounts_set_ui_ops (chatty_accounts_get_ui_ops ());
  purple_connections_set_ui_ops (chatty_connection_get_ui_ops ());
  purple_blist_set_ui_ops (chatty_blist_get_ui_ops ());
  purple_conversations_set_ui_ops (chatty_conversations_get_conv_ui_ops ());
}


static void
chatty_purple_prefs_init (void)
{
  purple_prefs_add_none (CHATTY_PREFS_ROOT "");
  purple_prefs_add_none ("/plugins/chatty");

  purple_prefs_add_none (CHATTY_PREFS_ROOT "/plugins");
  purple_prefs_add_bool (CHATTY_PREFS_ROOT "/plugins/message_carbons", TRUE);
  purple_prefs_add_path_list (CHATTY_PREFS_ROOT "/plugins/loaded", NULL);

  purple_prefs_add_none (CHATTY_PREFS_ROOT "/status");
  purple_prefs_add_bool (CHATTY_PREFS_ROOT "/status/first_start", TRUE);

  purple_prefs_add_none (CHATTY_PREFS_ROOT "/filelocations");
  purple_prefs_add_path (CHATTY_PREFS_ROOT "/filelocations/last_save_folder", "");
  purple_prefs_add_path (CHATTY_PREFS_ROOT "/filelocations/last_open_folder", "");
  purple_prefs_add_path (CHATTY_PREFS_ROOT "/filelocations/last_icon_folder", "");
}


static GHashTable *
chatty_purple_ui_get_info (void)
{
  if (NULL == ui_info) {
    ui_info = g_hash_table_new (g_str_hash, g_str_equal);

    g_hash_table_insert (ui_info, "name", CHATTY_APP_NAME);
    g_hash_table_insert (ui_info, "version", CHATTY_VERSION);
    g_hash_table_insert (ui_info, "dev_website", "https://source.puri.sm/Librem5/chatty");
    g_hash_table_insert (ui_info, "client_type", "phone");
  }

  return ui_info;
}


static
PurpleCoreUiOps core_ui_ops =
{
  chatty_purple_prefs_init,
  NULL,
  chatty_purple_ui_init,
  chatty_purple_quit,
  chatty_purple_ui_get_info,
  NULL,
  NULL,
  NULL
};


static PurpleCoreUiOps *
chatty_core_get_ui_ops (void)
{
  return &core_ui_ops;
}


gboolean
chatty_purple_load_plugin (const char *name)
{
  GList    *iter;
  gboolean  result = FALSE;

  iter = purple_plugins_get_all ();

  for (; iter; iter = iter->next) {
    PurplePlugin      *plugin = iter->data;
    PurplePluginInfo  *info = plugin->info;

    if (g_strcmp0 (info->id, name) == 0) {
      result = TRUE;
      g_debug ("Found plugin %s", info->name);

      if (!purple_plugin_is_loaded (plugin)) {
        result = purple_plugin_load (plugin);
        purple_plugins_save_loaded (CHATTY_PREFS_ROOT "/plugins/loaded");
        g_debug ("Loaded plugin %s", info->name);
      }
    }
  }

  return result;
}


gboolean
chatty_purple_unload_plugin (const char *name)
{
  PurplePlugin  *plugin;
  gboolean       result = FALSE;

  plugin = purple_plugins_find_with_id (name);

  if (plugin != NULL) {
    result = purple_plugin_unload (plugin);

    purple_plugin_disable (plugin);

    purple_plugins_save_loaded (CHATTY_PREFS_ROOT "/plugins/loaded");
  } else {
    g_debug ("Plugin %s couldn't be unloaded, it wasn't found", name);
    return FALSE;
  }

  if (result) {
    g_debug ("Unloaded plugin %s", name);
  } else {
    g_debug ("Plugin %s couldn't be unloaded now, "
             "it will be unloaded after a restart", name);
  }

  return result;
}



void
chatty_purple_check_sms_plugin (void)
{
  PurpleAccount *account;

  account = purple_accounts_find ("SMS", "prpl-mm-sms");

  if (account == NULL) {
    chatty_account_add_sms_account ();
  }
}


void
libpurple_init (void)
{
  gchar         *search_path;

  chatty_data_t *chatty = chatty_get_data ();

  chatty_purple_data_t *chatty_purple = chatty_get_purple_data ();

  signal (SIGCHLD, SIG_IGN);

  purple_debug_set_enabled (!!(chatty->cml_options & CHATTY_CML_OPT_DEBUG));
  purple_debug_set_verbose (!!(chatty->cml_options & CHATTY_CML_OPT_VERBOSE));

  purple_core_set_ui_ops (chatty_core_get_ui_ops ());
  purple_eventloop_set_ui_ops (chatty_eventloop_get_ui_ops ());

  search_path = g_build_filename (purple_user_dir (), "plugins", NULL);
  purple_plugins_add_search_path (search_path);
  g_free (search_path);

  if (!purple_core_init (CHATTY_UI)) {
    g_printerr ("libpurple initialization failed\n");

    g_application_quit (g_application_get_default ());
  }

  if (!purple_core_ensure_single_instance ()) {
    g_printerr ("Another libpurple client is already running\n");

    g_application_quit (g_application_get_default ());
  }

  purple_set_blist (purple_blist_new ());
  purple_prefs_load ();
  purple_blist_load ();
  purple_plugins_load_saved (CHATTY_PREFS_ROOT "/plugins/loaded");

  purple_plugins_probe (G_MODULE_SUFFIX);

  if (purple_plugins_find_with_id ("core-riba-carbons") != NULL) {
    chatty_purple->plugin_carbons_available = TRUE;
  } else {
    chatty_purple->plugin_carbons_available = FALSE;
  }

  if (purple_prefs_get_bool (CHATTY_PREFS_ROOT "/plugins/message_carbons")) {
    chatty_purple->plugin_carbons_loaded = chatty_purple_load_plugin ("core-riba-carbons");
  }

  chatty_purple->plugin_lurch_loaded = chatty_purple_load_plugin ("core-riba-lurch");

  purple_plugins_init ();
  purple_network_force_online();
  purple_pounces_load ();

  chatty_xeps_init ();

  chatty_purple->plugin_mm_sms_loaded = chatty_purple_load_plugin ("prpl-mm-sms");

  purple_savedstatus_activate (purple_savedstatus_get_startup());
  purple_accounts_restore_current_statuses ();

  purple_blist_show ();

  g_debug ("libpurple initialized. Running version %s.",
           purple_core_get_version ());
}
