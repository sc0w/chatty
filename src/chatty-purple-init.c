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
#include "./xeps/xeps.h"
#include "chatty-message-list.h"
#include "chatty-purple-request.h"
#include "chatty-purple-notify.h"
#include "chatty-buddy-list.h"
#include "chatty-connection.h"
#include "chatty-manager.h"
#include "chatty-conversation.h"
#include "chatty-folks.h"

static GHashTable *ui_info = NULL;

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


void
chatty_purple_quit (void)
{
  chatty_conversations_uninit ();
  chatty_blist_uninit ();

  purple_conversations_set_ui_ops (NULL);
  purple_connections_set_ui_ops (NULL);
  purple_blist_set_ui_ops (NULL);
  purple_accounts_set_ui_ops (NULL);

  if (NULL != ui_info) {
    g_hash_table_destroy (ui_info);
  }
  
  chatty_xeps_close ();
  chatty_folks_close ();
}


static void
chatty_purple_ui_init (void)
{
  chatty_blist_init ();
  chatty_conversations_init ();
  chatty_manager_purple_init (chatty_manager_get_default ());

  purple_accounts_set_ui_ops (chatty_accounts_get_ui_ops ());
  purple_request_set_ui_ops (chatty_request_get_ui_ops ());
  purple_notify_set_ui_ops (chatty_notify_get_ui_ops ());
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
  purple_prefs_add_path_list (CHATTY_PREFS_ROOT "/plugins/loaded", NULL);

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


void
libpurple_init (void)
{
  gchar         *search_path;

  signal (SIGCHLD, SIG_IGN);
  signal (SIGPIPE, SIG_IGN);

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

  chatty_manager_load_plugins (chatty_manager_get_default ());
  chatty_manager_load_buddies (chatty_manager_get_default ());

  purple_savedstatus_activate (purple_savedstatus_get_startup());
  purple_accounts_restore_current_statuses ();

  purple_blist_show ();

  g_debug ("libpurple initialized. Running version %s.",
           purple_core_get_version ());
}
