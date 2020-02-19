/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-manager.c
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-manager"

#include "chatty-config.h"

#include <purple.h>

#include "xeps/xeps.h"
#include "chatty-settings.h"
#include "contrib/gtk.h"
#include "chatty-account.h"
#include "chatty-contact-provider.h"
#include "chatty-utils.h"
#include "chatty-window.h"
#include "users/chatty-pp-account.h"
#include "chatty-chat.h"
#include "chatty-purple-init.h"
#include "chatty-manager.h"

/**
 * SECTION: chatty-manager
 * @title: ChattyManager
 * @short_description: A class to manage various providers and accounts
 * @include: "chatty-manager.h"
 *
 * libpurple doesn’t have a nice OOP interface for managing anthing.
 * This class hides all the complexities surrounding it.
 */

struct _ChattyManager
{
  GObject          parent_instance;

  ChattyFolks     *chatty_folks;
  GListStore      *account_list;
  GListStore      *chat_list;
  GListStore      *list_of_user_list;
  GtkFlattenListModel *contact_list;

  PurplePlugin    *sms_plugin;
  PurplePlugin    *lurch_plugin;
  PurplePlugin    *carbon_plugin;
  PurplePlugin    *file_upload_plugin;

  gboolean         disable_auto_login;
  gboolean         network_available;

  /* Number of accounts currently connected */
  guint            xmpp_count;
  guint            telegram_count;
  guint            matrix_count;
  gboolean         has_modem;
  ChattyProtocol   active_protocols;
};

G_DEFINE_TYPE (ChattyManager, chatty_manager, G_TYPE_OBJECT)

/* XXX: A copy from purple-mm-sms */
enum {
  PUR_MM_STATE_NO_MANAGER,
  PUR_MM_STATE_MANAGER_FOUND,
  PUR_MM_STATE_NO_MODEM,
  PUR_MM_STATE_MODEM_FOUND,
  PUR_MM_STATE_NO_MESSAGING_MODEM,
  PUR_MM_STATE_MODEM_DISABLED,
  PUR_MM_STATE_MODEM_UNLOCK_ERROR,
  PUR_MM_STATE_READY
} e_purple_connection;

enum {
  PROP_0,
  PROP_ACTIVE_PROTOCOLS,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];


static gboolean
chatty_manager_load_plugin (PurplePlugin *plugin)
{
  gboolean loaded;

  if (!plugin || purple_plugin_is_loaded (plugin))
    return TRUE;

  loaded = purple_plugin_load (plugin);
  purple_plugins_save_loaded (CHATTY_PREFS_ROOT "/plugins/loaded");
  g_debug ("plugin %s%s Loaded",
           purple_plugin_get_name (plugin),
           loaded ? "" : " Not");

  return loaded;
}

static void
chatty_manager_unload_plugin (PurplePlugin *plugin)
{
  gboolean unloaded;

  if (!plugin || !purple_plugin_is_loaded (plugin))
    return;

  unloaded = purple_plugin_unload (plugin);
  purple_plugin_disable (plugin);
  purple_plugins_save_loaded (CHATTY_PREFS_ROOT "/plugins/loaded");
  /* Failing to unload may mean that the application require restart to do so. */
  g_debug ("plugin %s%s Unloaded",
           purple_plugin_get_name (plugin),
           unloaded ? "" : " Not");
}

static void
manager_message_carbons_changed (ChattyManager  *self,
                                 GParamSpec     *pspec,
                                 ChattySettings *settings)
{
  g_assert (CHATTY_IS_MANAGER (self));
  g_assert (CHATTY_IS_SETTINGS (settings));

  if (!self->carbon_plugin)
    return;

  if (chatty_settings_get_message_carbons (settings))
    chatty_manager_load_plugin (self->carbon_plugin);
  else
    chatty_manager_unload_plugin (self->carbon_plugin);
}

static void
chatty_manager_enable_sms_account (ChattyManager *self)
{
  g_autoptr(ChattyPpAccount) account = NULL;

  if (purple_accounts_find ("SMS", "prpl-mm-sms"))
    return;

  account = chatty_pp_account_new (CHATTY_PROTOCOL_SMS, "SMS", NULL);
  chatty_pp_account_save (account);
}

static ChattyPpBuddy *
manager_find_buddy (GListModel  *model,
                    PurpleBuddy *pp_buddy)
{
  guint n_items;

  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++) {
    g_autoptr(ChattyPpBuddy) buddy = NULL;

    buddy = g_list_model_get_item (model, i);

    if (chatty_pp_buddy_get_buddy (buddy) == pp_buddy)
      return buddy;
  }

  return NULL;
}

static void
manager_buddy_added_cb (PurpleBuddy   *pp_buddy,
                        ChattyManager *self)
{
  ChattyPpAccount *account;
  ChattyPpBuddy *buddy;
  PurpleAccount *pp_account;
  GListModel *model;

  g_assert (CHATTY_IS_MANAGER (self));

  pp_account = purple_buddy_get_account (pp_buddy);
  account = chatty_pp_account_get_object (pp_account);
  g_return_if_fail (account);

  model = chatty_pp_account_get_buddy_list (account);
  buddy = manager_find_buddy (model, pp_buddy);

  if (!buddy)
    chatty_pp_account_add_purple_buddy (account, pp_buddy);
}

static void
manager_buddy_removed_cb (PurpleBuddy   *pp_buddy,
                          ChattyManager *self)
{
  ChattyPpAccount *account;
  PurpleAccount *pp_account;
  ChattyPpBuddy *buddy;
  GListModel *model;

  g_assert (CHATTY_IS_MANAGER (self));

  pp_account = purple_buddy_get_account (pp_buddy);
  account = chatty_pp_account_get_object (pp_account);

  g_return_if_fail (account);
  model = chatty_pp_account_get_buddy_list (account);
  buddy = manager_find_buddy (model, pp_buddy);

  g_return_if_fail (buddy);

  g_signal_emit_by_name (buddy, "deleted");
  chatty_utils_remove_list_item (G_LIST_STORE (model), buddy);
}

static void
manager_account_added_cb (PurpleAccount *pp_account,
                          ChattyManager *self)
{
  g_autoptr(ChattyPpAccount) account = NULL;

  g_assert (CHATTY_IS_MANAGER (self));

  account = chatty_pp_account_get_object (pp_account);

  if (account)
    g_object_ref (account);
  else
    account = chatty_pp_account_new_purple (pp_account);

  g_object_notify (G_OBJECT (account), "status");
  g_list_store_append (self->account_list, account);
  g_list_store_append (self->list_of_user_list,
                       chatty_pp_account_get_buddy_list (account));

  if (self->disable_auto_login)
    chatty_account_set_enabled (CHATTY_ACCOUNT (account), FALSE);

  if (chatty_pp_account_is_sms (account))
    chatty_account_set_enabled (CHATTY_ACCOUNT (account), TRUE);
}

static void
manager_account_removed_cb (PurpleAccount *pp_account,
                            ChattyManager *self)
{
  ChattyPpAccount *account;

  g_assert (CHATTY_IS_MANAGER (self));

  /* account should exist in the store */
  account = chatty_pp_account_get_object (pp_account);
  g_return_if_fail (account);

  chatty_utils_remove_list_item (self->list_of_user_list,
                                 chatty_pp_account_get_buddy_list (account));
  g_object_notify (G_OBJECT (account), "status");
  g_signal_emit_by_name (account, "deleted");
  chatty_utils_remove_list_item (self->account_list, account);
}

static void
manager_account_changed_cb (PurpleAccount *pp_account,
                            ChattyManager *self)
{
  ChattyPpAccount *account;

  account = chatty_pp_account_get_object (pp_account);
  g_return_if_fail (account);

  g_object_notify (G_OBJECT (account), "enabled");
}

static void
manager_account_connection_failed_cb (PurpleAccount         *pp_account,
                                      PurpleConnectionError  error,
                                      const gchar           *error_msg,
                                      ChattyManager         *self)
{
  ChattyPpAccount *account;

  g_assert (CHATTY_IS_MANAGER (self));

  /* account should exist in the store */
  account = chatty_pp_account_get_object (pp_account);
  g_return_if_fail (account);

  if (error == PURPLE_CONNECTION_ERROR_NETWORK_ERROR &&
      self->network_available)
    chatty_pp_account_connect (account, TRUE);
}

static void
manager_connection_changed_cb (PurpleConnection *gc,
                               ChattyManager    *self)
{
  PurpleAccount *pp_account;
  ChattyPpAccount *account;

  g_assert (CHATTY_IS_MANAGER (self));

  pp_account = purple_connection_get_account (gc);
  account = chatty_pp_account_get_object (pp_account);

  if (account)
    g_object_notify (G_OBJECT (account), "status");
  else
    g_return_if_reached ();
}

static void
manager_connection_signed_on_cb (PurpleConnection *gc,
                                 ChattyManager    *self)
{
  PurpleAccount *pp_account;
  ChattyPpAccount *account;
  ChattyProtocol protocol, old_protocols;

  g_assert (CHATTY_IS_MANAGER (self));

  pp_account = purple_connection_get_account (gc);
  account = chatty_pp_account_get_object (pp_account);
  g_return_if_fail (account);

  /*
   * SMS plugin emits “signed-on” regardless of the true state
   * So it’s handled in “mm-sms-state” callback.
   */
  if (chatty_pp_account_is_sms (account))
    return;

  protocol = chatty_user_get_protocols (CHATTY_USER (account));
  old_protocols = self->active_protocols;
  self->active_protocols |= protocol;

  if (protocol & CHATTY_PROTOCOL_XMPP)
    self->xmpp_count++;
  if (protocol & CHATTY_PROTOCOL_MATRIX)
    self->matrix_count++;
  if (protocol & CHATTY_PROTOCOL_TELEGRAM)
    self->telegram_count++;

  if (old_protocols != self->active_protocols)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE_PROTOCOLS]);

  g_object_notify (G_OBJECT (account), "status");
}

static void
manager_connection_signed_off_cb (PurpleConnection *gc,
                                  ChattyManager    *self)
{
  PurpleAccount *pp_account;
  ChattyPpAccount *account;
  ChattyProtocol protocol, old_protocols;

  g_assert (CHATTY_IS_MANAGER (self));

  pp_account = purple_connection_get_account (gc);
  account = chatty_pp_account_get_object (pp_account);
  g_return_if_fail (account);

  /*
   * SMS plugin emits “signed-off” regardless of the true state
   * So it’s handled in “mm-sms-state” callback.
   */
  if (chatty_pp_account_is_sms (account))
    return;

  protocol = chatty_user_get_protocols (CHATTY_USER (account));
  old_protocols = self->active_protocols;
  self->active_protocols = CHATTY_PROTOCOL_NONE;

  if (protocol & CHATTY_PROTOCOL_XMPP)
    if (self->xmpp_count > 0)
      self->xmpp_count--;
  if (protocol & CHATTY_PROTOCOL_MATRIX)
    if (self->matrix_count > 0)
      self->matrix_count--;
  if (protocol & CHATTY_PROTOCOL_TELEGRAM)
    if (self->telegram_count > 0)
      self->telegram_count--;

  if (self->xmpp_count)
    self->active_protocols |= CHATTY_PROTOCOL_XMPP;
  if (self->matrix_count)
    self->active_protocols |= CHATTY_PROTOCOL_MATRIX;
  if (self->telegram_count)
    self->active_protocols |= CHATTY_PROTOCOL_TELEGRAM;
  if (self->has_modem)
    self->active_protocols |= CHATTY_PROTOCOL_SMS;

  if (old_protocols != self->active_protocols)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE_PROTOCOLS]);

  g_object_notify (G_OBJECT (account), "status");
}

static ChattyChat *
manager_find_chat (GListModel *model,
                   PurpleChat *pp_chat)
{
  guint n_items;

  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++) {
    g_autoptr(ChattyChat) chat = NULL;

    chat = g_list_model_get_item (model, i);

    if (chatty_chat_get_purple_chat (chat) == pp_chat)
      return chat;
  }

  return NULL;
}

static void
manager_sms_modem_added_cb (gint status)
{
  ChattyPpAccount *account;
  PurpleAccount   *pp_account;

  pp_account = purple_accounts_find ("SMS", "prpl-mm-sms");
  account = chatty_pp_account_get_object (pp_account);
  g_return_if_fail (CHATTY_IS_PP_ACCOUNT (account));

  chatty_pp_account_connect (account, TRUE);
}


/* XXX: works only with one modem */
static void
manager_sms_state_changed_cb (int            state,
                              ChattyManager *self)
{
  ChattyProtocol old_protocols;

  g_assert (CHATTY_IS_MANAGER (self));

  old_protocols = self->active_protocols;

  if (state == PUR_MM_STATE_READY) {
    self->has_modem = TRUE;
    self->active_protocols |= CHATTY_PROTOCOL_SMS;
  } else if (state != PUR_MM_STATE_MANAGER_FOUND && state != PUR_MM_STATE_MODEM_FOUND) {
    self->has_modem = FALSE;
    self->active_protocols &= ~CHATTY_PROTOCOL_SMS;
  }

  if (old_protocols != self->active_protocols)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE_PROTOCOLS]);
}

static void
manager_network_changed_cb (GNetworkMonitor *network_monitor,
                            gboolean         network_available,
                            ChattyManager   *self)
{
  GListModel *list;
  guint n_items;

  g_assert (G_IS_NETWORK_MONITOR (network_monitor));
  g_assert (CHATTY_IS_MANAGER (self));

  if (network_available == self->network_available)
    return;

  self->network_available = network_available;
  list = G_LIST_MODEL (self->account_list);
  n_items = g_list_model_get_n_items (list);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(ChattyPpAccount) account = NULL;

      account = g_list_model_get_item (list, i);

      if (network_available)
        chatty_pp_account_connect (account, FALSE);
      else
        chatty_pp_account_disconnect (account);
    }
}

static void
chatty_manager_intialize_libpurple (ChattyManager *self)
{
  GNetworkMonitor *network_monitor;

  g_assert (CHATTY_IS_MANAGER (self));

  network_monitor = g_network_monitor_get_default ();
  self->network_available = g_network_monitor_get_network_available (network_monitor);

  purple_signal_connect (purple_accounts_get_handle(),
                         "account-added", self,
                         PURPLE_CALLBACK (manager_account_added_cb), self);

  purple_signal_connect (purple_accounts_get_handle(),
                         "account-removed", self,
                         PURPLE_CALLBACK (manager_account_removed_cb), self);

  for (GList *node = purple_accounts_get_all (); node; node = node->next)
    manager_account_added_cb (node->data, self);

  purple_signal_connect (purple_accounts_get_handle(),
                         "account-enabled", self,
                         PURPLE_CALLBACK (manager_account_changed_cb), self);
  purple_signal_connect (purple_accounts_get_handle(),
                         "account-disabled", self,
                         PURPLE_CALLBACK (manager_account_changed_cb), self);
  purple_signal_connect (purple_accounts_get_handle(),
                         "account-connection-error", self,
                         PURPLE_CALLBACK (manager_account_connection_failed_cb), self);

  purple_signal_connect (purple_connections_get_handle(),
                         "signing-on", self,
                         PURPLE_CALLBACK (manager_connection_changed_cb), self);
  purple_signal_connect (purple_connections_get_handle(),
                         "signed-on", self,
                         PURPLE_CALLBACK (manager_connection_signed_on_cb), self);
  purple_signal_connect (purple_connections_get_handle(),
                         "signed-off", self,
                         PURPLE_CALLBACK (manager_connection_signed_off_cb), self);

  purple_signal_connect (purple_plugins_get_handle (),
                         "mm-sms-modem-added", self,
                         PURPLE_CALLBACK (manager_sms_modem_added_cb), NULL);

  purple_signal_connect (purple_plugins_get_handle (),
                         "mm-sms-state", self,
                         PURPLE_CALLBACK (manager_sms_state_changed_cb), self);

  g_signal_connect_object (network_monitor, "network-changed",
                           G_CALLBACK (manager_network_changed_cb), self,
                           G_CONNECT_AFTER);
}


static void
chatty_manager_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  ChattyManager *self = (ChattyManager *)object;

  switch (prop_id)
    {
    case PROP_ACTIVE_PROTOCOLS:
      g_value_set_int (value, chatty_manager_get_active_protocols (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
chatty_manager_dispose (GObject *object)
{
  ChattyManager *self = (ChattyManager *)object;

  purple_signals_disconnect_by_handle (self);
  g_clear_object (&self->contact_list);
  g_clear_object (&self->list_of_user_list);
  g_clear_object (&self->account_list);

  G_OBJECT_CLASS (chatty_manager_parent_class)->dispose (object);
}

static void
chatty_manager_class_init (ChattyManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = chatty_manager_get_property;
  object_class->dispose = chatty_manager_dispose;

  /**
   * ChattyUser:active-protocols:
   *
   * Protocols currently available for use.  This is a
   * flag of protocols currently connected and available
   * for use.
   */
  properties[PROP_ACTIVE_PROTOCOLS] =
    g_param_spec_int ("active-protocols",
                      "Active protocols",
                      "Protocols currently active and connected",
                      CHATTY_PROTOCOL_NONE,
                      CHATTY_PROTOCOL_TELEGRAM,
                      CHATTY_PROTOCOL_NONE,
                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

}

static void
chatty_manager_init (ChattyManager *self)
{
  self->chatty_folks = chatty_folks_new ();

  self->account_list = g_list_store_new (CHATTY_TYPE_PP_ACCOUNT);
  self->chat_list = g_list_store_new (CHATTY_TYPE_CHAT);
  self->list_of_user_list = g_list_store_new (G_TYPE_LIST_MODEL);
  self->contact_list = gtk_flatten_list_model_new (G_TYPE_OBJECT,
                                                   G_LIST_MODEL (self->list_of_user_list));
  g_list_store_append (self->list_of_user_list, G_LIST_MODEL (self->chat_list));
  g_list_store_append (self->list_of_user_list,
                       chatty_folks_get_model (self->chatty_folks));
}

ChattyManager *
chatty_manager_get_default (void)
{
  static ChattyManager *self;

  if (!self)
    {
      self = g_object_new (CHATTY_TYPE_MANAGER, NULL);
      g_object_add_weak_pointer (G_OBJECT (self), (gpointer *)&self);
    }

  return self;
}

/* XXX: Remove once the dust settles */
void
chatty_manager_purple_init (ChattyManager *self)
{
  g_return_if_fail (CHATTY_IS_MANAGER (self));

  if (!self->disable_auto_login)
    purple_savedstatus_activate (purple_savedstatus_new (NULL, PURPLE_STATUS_AVAILABLE));

  chatty_manager_intialize_libpurple (self);
}

GListModel *
chatty_manager_get_accounts (ChattyManager *self)
{
  g_return_val_if_fail (CHATTY_IS_MANAGER (self), NULL);

  return G_LIST_MODEL (self->account_list);
}

GListModel *
chatty_manager_get_contact_list (ChattyManager *self)
{
  g_return_val_if_fail (CHATTY_IS_MANAGER (self), NULL);

  return G_LIST_MODEL (self->contact_list);
}

/**
 * chatty_manager_disable_auto_login:
 * @self: A #ChattyManager
 * @disable: whether to disable auto-login
 *
 * Set whether to disable automatic login when accounts are
 * loaded/added.  By default, auto-login is enabled if the
 * account is enabled with chatty_pp_account_set_enabled().
 *
 * This is not applicable to SMS accounts.
 */
void
chatty_manager_disable_auto_login (ChattyManager *self,
                                   gboolean       disable)
{
  g_return_if_fail (CHATTY_IS_MANAGER (self));

  self->disable_auto_login = !!disable;
}

gboolean
chatty_manager_get_disable_auto_login (ChattyManager *self)
{
  g_return_val_if_fail (CHATTY_IS_MANAGER (self), TRUE);

  return self->disable_auto_login;
}

void
chatty_manager_load_plugins (ChattyManager *self)
{
  ChattySettings *settings;

  g_return_if_fail (CHATTY_IS_MANAGER (self));

  purple_plugins_load_saved (CHATTY_PREFS_ROOT "/plugins/loaded");
  purple_plugins_probe (G_MODULE_SUFFIX);

  self->sms_plugin = purple_plugins_find_with_id ("prpl-mm-sms");
  self->lurch_plugin = purple_plugins_find_with_id ("core-riba-lurch");
  self->carbon_plugin = purple_plugins_find_with_id ("core-riba-carbons");
  self->file_upload_plugin = purple_plugins_find_with_id ("xep-http-file-upload");

  chatty_manager_load_plugin (self->lurch_plugin);
  chatty_manager_load_plugin (self->file_upload_plugin);

  purple_plugins_init ();
  purple_network_force_online();
  purple_pounces_load ();

  chatty_xeps_init ();

  if (chatty_manager_load_plugin (self->sms_plugin))
    chatty_manager_enable_sms_account (self);

  settings = chatty_settings_get_default ();
  g_signal_connect_object (settings, "notify::message-carbons",
                           G_CALLBACK (manager_message_carbons_changed), self,
                           G_CONNECT_SWAPPED);
  manager_message_carbons_changed (self, NULL, settings);
}

void
chatty_manager_load_buddies (ChattyManager *self)
{
  g_autoptr(GSList) buddies = NULL;

  g_return_if_fail (CHATTY_IS_MANAGER (self));

  purple_signal_connect (purple_blist_get_handle (),
                         "buddy-added", self,
                         PURPLE_CALLBACK (manager_buddy_added_cb), self);
  purple_signal_connect (purple_blist_get_handle (),
                         "buddy-removed", self,
                         PURPLE_CALLBACK (manager_buddy_removed_cb), self);

  buddies = purple_blist_get_buddies ();

  for (GSList *node = buddies; node; node = node->next)
    manager_buddy_added_cb (node->data, self);
}

gboolean
chatty_manager_has_carbons_plugin (ChattyManager *self)
{
  g_return_val_if_fail (CHATTY_IS_MANAGER (self), FALSE);

  return self->carbon_plugin != NULL;
}

gboolean
chatty_manager_has_file_upload_plugin (ChattyManager *self)
{
  g_return_val_if_fail (CHATTY_IS_MANAGER (self), FALSE);

  return self->file_upload_plugin != NULL;
}

gboolean
chatty_manager_lurch_plugin_is_loaded (ChattyManager *self)
{
  g_return_val_if_fail (CHATTY_IS_MANAGER (self), FALSE);

  if (!self->lurch_plugin)
    return FALSE;

  return purple_plugin_is_loaded (self->lurch_plugin);
}

ChattyProtocol
chatty_manager_get_active_protocols (ChattyManager *self)
{
  g_return_val_if_fail (CHATTY_IS_MANAGER (self), CHATTY_PROTOCOL_NONE);

  return self->active_protocols;
}

ChattyFolks *
chatty_manager_get_folks (ChattyManager *self)
{
  g_return_val_if_fail (CHATTY_IS_MANAGER (self), NULL);

  return self->chatty_folks;
}


void
chatty_manager_update_node (ChattyManager   *self,
                            PurpleBlistNode *node)
{
  g_autoptr(ChattyChat) chat = NULL;
  PurpleChat *pp_chat;

  g_assert (CHATTY_IS_MANAGER (self));

  if (!PURPLE_BLIST_NODE_IS_CHAT (node))
    return;

  pp_chat = (PurpleChat*)node;

  if(!purple_account_is_connected (pp_chat->account))
    return;

  chat = manager_find_chat (G_LIST_MODEL (self->chat_list), pp_chat);

  if (chat) {
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE_PROTOCOLS]);
    chat = NULL;

    return;
  }

  chat = chatty_chat_new_purple_chat (pp_chat);
  g_list_store_append (self->chat_list, chat);
}


void
chatty_manager_remove_node (ChattyManager   *self,
                            PurpleBlistNode *node)
{
  ChattyChat *chat;
  PurpleChat *pp_chat;

  g_assert (CHATTY_IS_MANAGER (self));

  if (!PURPLE_BLIST_NODE_IS_CHAT (node))
    return;

  pp_chat = (PurpleChat*)node;

  chat = manager_find_chat (G_LIST_MODEL (self->chat_list), pp_chat);

  if (chat)
    chatty_utils_remove_list_item (self->chat_list, chat);
}


static ChattyPpBuddy *
manager_find_buddy_from_contact (GListModel  *model,
                                 PurpleBuddy *pp_buddy)
{
  guint n_items;

  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++) {
    g_autoptr(GObject) buddy = NULL;

    buddy = g_list_model_get_item (model, i);

    if (CHATTY_IS_PP_BUDDY (buddy))
      if (chatty_pp_buddy_get_buddy (CHATTY_PP_BUDDY (buddy)) == pp_buddy)
        return CHATTY_PP_BUDDY (buddy);
  }

  return NULL;
}


void
chatty_manager_emit_changed (ChattyManager   *self,
                             PurpleBlistNode *node)
{
  ChattyPpAccount *account;
  ChattyPpBuddy *buddy;
  PurpleAccount *pp_account;
  PurpleBuddy *pp_buddy;

  g_return_if_fail (CHATTY_IS_MANAGER (self));

  if (!PURPLE_BLIST_NODE_IS_BUDDY (node))
    return;

  pp_buddy = (PurpleBuddy *)node;
  buddy = manager_find_buddy_from_contact (G_LIST_MODEL (self->contact_list), pp_buddy);

  if (!buddy)
    return;

  pp_account = chatty_pp_buddy_get_account (buddy);
  account = chatty_pp_account_get_object (pp_account);

  /*
   * HACK: remove and add the item so that the related widget is recreated with updated values
   * This is required until we use ChattyAvatar widget for avatar.
   */
  if (chatty_utils_get_item_position (chatty_pp_account_get_buddy_list (account), buddy, NULL))
    g_signal_emit_by_name (buddy, "changed");
}
