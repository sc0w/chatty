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
#include "chatty-contact-provider.h"
#include "chatty-utils.h"
#include "chatty-window.h"
#include "chatty-chat-view.h"
#include "users/chatty-pp-account.h"
#include "chatty-chat.h"
#include "chatty-purple-request.h"
#include "chatty-purple-notify.h"
#include "chatty-purple-init.h"
#include "chatty-conversation.h"
#include "chatty-history.h"
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

  ChattyEds       *chatty_eds;
  GListStore      *account_list;
  GListStore      *chat_list;
  GListStore      *im_list;
  GListStore      *list_of_chat_list;
  GListStore      *list_of_user_list;
  GtkFlattenListModel *contact_list;
  GtkFlattenListModel *chat_im_list;
  GtkSortListModel    *sorted_chat_im_list;
  GtkSorter           *chat_sorter;

  PurplePlugin    *sms_plugin;
  PurplePlugin    *lurch_plugin;
  PurplePlugin    *carbon_plugin;
  PurplePlugin    *file_upload_plugin;

  gboolean         disable_auto_login;
  gboolean         network_available;

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

enum {
  AUTHORIZE_BUDDY,
  NOTIFY_ADDED,
  CONNECTION_ERROR,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];


static int
manager_sort_chat_item (ChattyChat *a,
                        ChattyChat *b,
                        gpointer    user_data)
{
  time_t a_time, b_time;

  g_assert (CHATTY_IS_CHAT (a));
  g_assert (CHATTY_IS_CHAT (b));

  a_time = chatty_chat_get_last_msg_time (a);
  b_time = chatty_chat_get_last_msg_time (b);

  return difftime (b_time, a_time);
}

static void
manager_eds_is_ready (ChattyManager *self)
{
  GListModel *accounts, *model;
  ChattyContact *contact;
  const char *id;
  ChattyProtocol protocol;
  guint n_accounts, n_buddies;

  g_assert (CHATTY_IS_MANAGER (self));

  accounts = chatty_manager_get_accounts (self);
  n_accounts = g_list_model_get_n_items (accounts);

  /* TODO: Optimize */
  for (guint i = 0; i < n_accounts; i++) {
    g_autoptr(ChattyPpAccount) account = NULL;

    account  = g_list_model_get_item (accounts, i);
    protocol = chatty_item_get_protocols (CHATTY_ITEM (account));

    if (protocol != CHATTY_PROTOCOL_SMS)
      continue;

    model = chatty_pp_account_get_buddy_list (account);
    n_buddies = g_list_model_get_n_items (model);
    for (guint j = 0; j < n_buddies; j++) {
      g_autoptr(ChattyPpBuddy) buddy = NULL;

      buddy = g_list_model_get_item (model, j);
      id = chatty_pp_buddy_get_id (buddy);

      if (chatty_pp_buddy_get_contact (buddy))
        continue;

      contact = chatty_eds_find_by_number (self->chatty_eds, id);

      chatty_pp_buddy_set_contact (buddy, contact);
    }
  }
}

static void
chatty_manager_account_notify_added (PurpleAccount *pp_account,
                                     const char    *remote_user,
                                     const char    *id,
                                     const char    *alias,
                                     const char    *msg)
{
  ChattyManager *self;
  ChattyPpAccount *account;

  self = chatty_manager_get_default ();
  account = chatty_pp_account_get_object (pp_account);
  g_signal_emit (self,  signals[NOTIFY_ADDED], 0, account, remote_user, id);
}


static void *
chatty_manager_account_request_authorization (PurpleAccount *pp_account,
                                              const char    *remote_user,
                                              const char    *id,
                                              const char    *alias,
                                              const char    *message,
                                              gboolean       on_list,
                                              PurpleAccountRequestAuthorizationCb auth_cb,
                                              PurpleAccountRequestAuthorizationCb deny_cb,
                                              void          *user_data)
{
  ChattyManager *self;
  ChattyPpAccount *account;
  GtkResponseType  response = GTK_RESPONSE_CANCEL;

  self = chatty_manager_get_default ();
  account = chatty_pp_account_get_object (pp_account);
  g_signal_emit (self,  signals[AUTHORIZE_BUDDY], 0, account, remote_user,
                 alias ? alias : remote_user, message, &response);

  if (response == GTK_RESPONSE_ACCEPT) {
    if (!on_list)
      purple_blist_request_add_buddy (pp_account, remote_user, NULL, alias);
    auth_cb (user_data);
  } else {
    deny_cb (user_data);
  }

  g_debug ("Request authorization user: %s alias: %s", remote_user, alias);

  return NULL;
}


static void
chatty_manager_account_request_add (PurpleAccount *account,
                                    const char    *remote_user,
                                    const char    *id,
                                    const char    *alias,
                                    const char    *msg)
{
  PurpleConnection *gc;

  gc = purple_account_get_connection (account);

  if (g_list_find (purple_connections_get_all (), gc))
    purple_blist_request_add_buddy (account, remote_user, NULL, alias);

  g_debug ("chatty_manager_account_request_add");
}


static PurpleAccountUiOps ui_ops =
{
  chatty_manager_account_notify_added,
  NULL,
  chatty_manager_account_request_add,
  chatty_manager_account_request_authorization,
};


static void
chatty_blist_remove (PurpleBuddyList *list,
                     PurpleBlistNode *node)
{
  if (CHATTY_PP_BUDDY (node->ui_data))
    g_signal_emit_by_name (node->ui_data, "deleted");

  purple_request_close_with_handle (node);

  if (node->ui_data)
    purple_signals_disconnect_by_handle (node->ui_data);

  chatty_manager_remove_node (chatty_manager_get_default (), node);
}


static void
chatty_blist_update_buddy (PurpleBuddyList *list,
                           PurpleBlistNode *node)
{
  PurpleBuddy             *buddy;
  g_autofree ChattyLog    *log_data = NULL;
  PurpleAccount           *account;
  const char              *username;
  g_autofree char         *who;
  char                     message_exists;

  g_return_if_fail (PURPLE_BLIST_NODE_IS_BUDDY(node));

  buddy = (PurpleBuddy*)node;

  account = purple_buddy_get_account (buddy);

  username = purple_account_get_username (account);
  who = chatty_utils_jabber_id_strip (purple_buddy_get_name (buddy));
  log_data = g_new0(ChattyLog, 1);

  message_exists = chatty_history_get_im_last_message (username, who, log_data);
  if (!message_exists && !purple_blist_node_get_bool (node, "chatty-notifications"))
    purple_blist_node_set_bool (node, "chatty-notifications", TRUE);

  if (purple_blist_node_get_bool (node, "chatty-autojoin") &&
      purple_account_is_connected (buddy->account) &&
      message_exists) {
    g_autoptr(ChattyChat) chat = NULL;
    ChattyChat *item;

    chat = chatty_chat_new_im_chat (account, buddy);
    item = chatty_manager_add_chat (chatty_manager_get_default (), chat);
    chatty_chat_set_last_message (item, log_data->msg);
    chatty_chat_set_last_msg_direction (item, log_data->dir);
    chatty_chat_set_last_msg_time (item, log_data->epoch);
  }
}


static void
chatty_blist_update (PurpleBuddyList *list,
                     PurpleBlistNode *node)
{
  if (!node)
    return;

  switch (node->type) {
  case PURPLE_BLIST_BUDDY_NODE:
    chatty_blist_update_buddy (list, node);
    chatty_manager_emit_changed (chatty_manager_get_default (), node);

    break;
  case PURPLE_BLIST_CHAT_NODE:
    chatty_manager_update_node (chatty_manager_get_default (), node);
    break;

  case PURPLE_BLIST_CONTACT_NODE:
  case PURPLE_BLIST_GROUP_NODE:
  case PURPLE_BLIST_OTHER_NODE:
  default:
    return;
  }
}


static void
chatty_blist_request_add_buddy (PurpleAccount *account,
                                const char    *username,
                                const char    *group,
                                const char    *alias)
{
  PurpleBuddy     *buddy;
  const char      *account_name;

  buddy = purple_find_buddy (account, username);

  if (buddy == NULL) {
    buddy = purple_buddy_new (account, username, alias);

    purple_blist_add_buddy (buddy, NULL, NULL, NULL);
    purple_blist_node_set_bool (PURPLE_BLIST_NODE(buddy), "chatty-notifications", TRUE);
  }

  purple_account_add_buddy (account, buddy);

  account_name = purple_account_get_username (account);

  g_debug ("chatty_blist_request_add_buddy: %s  %s  %s",
           account_name, username, alias);
}


static PurpleBlistUiOps blist_ui_ops =
{
  NULL,
  NULL,
  NULL,
  chatty_blist_update,
  chatty_blist_remove,
  NULL,
  NULL,
  chatty_blist_request_add_buddy,
};


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
  ChattyContact *contact;
  PurpleAccount *pp_account;
  GListModel *model;
  const char *id;

  g_assert (CHATTY_IS_MANAGER (self));

  pp_account = purple_buddy_get_account (pp_buddy);
  account = chatty_pp_account_get_object (pp_account);
  g_return_if_fail (account);

  model = chatty_pp_account_get_buddy_list (account);
  buddy = manager_find_buddy (model, pp_buddy);

  if (!buddy)
    buddy = chatty_pp_account_add_purple_buddy (account, pp_buddy);

  id = chatty_pp_buddy_get_id (buddy);
  contact = chatty_eds_find_by_number (self->chatty_eds, id);
  chatty_pp_buddy_set_contact (buddy, contact);
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

  /*
   * If account is NULL, the account has gotten deleted, and so
   * the buddy object is also deleted along it.
   */
  if (!account)
    return;

  model = chatty_pp_account_get_buddy_list (account);
  buddy = manager_find_buddy (model, pp_buddy);

  g_return_if_fail (buddy);

  g_signal_emit_by_name (buddy, "deleted");
  chatty_utils_remove_list_item (G_LIST_STORE (model), buddy);
}


static void
manager_buddy_privacy_chaged_cb (PurpleBuddy *buddy)
{
  if (!PURPLE_BLIST_NODE(buddy)->ui_data)
    return;

  chatty_blist_update (purple_get_blist (), PURPLE_BLIST_NODE(buddy));
}


static void
manager_buddy_signed_on_off_cb (PurpleBuddy *buddy)
{
  chatty_blist_update (purple_get_blist(), (PurpleBlistNode*)buddy);

  g_debug ("Buddy \"%s\"\n (%s) signed on/off", purple_buddy_get_name (buddy),
           purple_account_get_protocol_id (purple_buddy_get_account(buddy)));
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

  if (purple_connection_error_is_fatal (error))
    g_signal_emit (self,  signals[CONNECTION_ERROR], 0, account, error_msg);
}


static void
manager_conversation_created_cb (PurpleConversation *conv,
                                 ChattyManager      *self)
{
  if (conv->type == PURPLE_CONV_TYPE_CHAT &&
      !purple_blist_find_chat (conv->account, conv->name))
    return;

  chatty_manager_add_conversation (self, conv);
}


static void
manager_conversation_updated_cb (PurpleConversation   *conv,
                                 PurpleConvUpdateType  type,
                                 ChattyManager        *self)
{
  ChattyChat  *chat;
  PurpleBuddy *buddy;

  if (type != PURPLE_CONV_UPDATE_UNSEEN || !conv->name)
    return;

  buddy = purple_find_buddy (conv->account, conv->name);
  chat  = chatty_manager_find_purple_conv (self, conv);

  if(buddy) {
    chatty_blist_update (NULL, (PurpleBlistNode *)buddy);
  } else if (chat) {
    chatty_chat_set_last_msg_time (chat, time (NULL));
    chatty_chat_set_unread_count (chat, chatty_chat_get_unread_count (chat) + 1);
    gtk_sorter_changed (self->chat_sorter, GTK_SORTER_ORDER_TOTAL);
  }
}


static void
manager_deleting_conversation_cb (PurpleConversation *conv,
                                  ChattyManager      *self)
{
  chatty_manager_delete_conversation (self, conv);
}


static void
manager_wrote_chat_im_msg_cb (PurpleAccount      *account,
                              const char         *who,
                              const char         *message,
                              PurpleConversation *conv,
                              PurpleMessageFlags  flag,
                              ChattyManager      *self)
{
  PurpleBlistNode *node = NULL;
  ChattyChat *chat;

  chat = chatty_manager_find_purple_conv (self, conv);

  if (chat && (flag & PURPLE_MESSAGE_RECV))
    chatty_chat_set_unread_count (chat, chatty_chat_get_unread_count (chat) + 1);

  if (chat)
    node = (PurpleBlistNode *)chatty_chat_get_purple_buddy (chat);

  if (node)
    chatty_chat_set_last_message (chat, message);

  chatty_chat_set_last_msg_time (chat, time (NULL));
  gtk_sorter_changed (self->chat_sorter, GTK_SORTER_ORDER_TOTAL);
}


static gboolean
manager_conversation_buddy_leaving_cb (PurpleConversation *conv,
                                       const char         *user,
                                       const char         *reason,
                                       ChattyManager      *self)
{
  ChattyChat *chat;

  g_assert (CHATTY_IS_MANAGER (self));

  chat = chatty_manager_find_purple_conv (self, conv);
  g_return_val_if_fail (chat, TRUE);

  chatty_chat_remove_user (chat, user);

  return TRUE;
}

static gboolean
auto_join_chat_cb (gpointer data)
{
  PurpleBlistNode  *node;
  PurpleConnection *pc = data;
  GHashTable               *components;
  PurplePluginProtocolInfo *prpl_info;
  PurpleAccount    *account = purple_connection_get_account (pc);

  for (node = purple_blist_get_root (); node;
       node = purple_blist_node_next (node, FALSE)) {

    if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
      PurpleChat *chat = (PurpleChat*)node;

      if (purple_chat_get_account (chat) == account &&
          purple_blist_node_get_bool (node, "chatty-autojoin")) {
        g_autofree char *chat_name;

        prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_find_prpl (purple_account_get_protocol_id (account)));
        components = purple_chat_get_components (chat);
        chat_name = prpl_info->get_chat_name(components);
        chatty_conv_add_history_since_component(components, account->username, chat_name);

        serv_join_chat (purple_account_get_connection (account),
                        purple_chat_get_components (chat));
      }
    }
  }

  return FALSE;
}


static gboolean
manager_connection_autojoin_cb (PurpleConnection *gc,
                                gpointer          user_data)
{
  g_idle_add (auto_join_chat_cb, gc);

  return TRUE;
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
manager_update_protocols (ChattyManager *self)
{
  GListModel *model;
  ChattyProtocol protocol;
  ChattyStatus status;
  guint n_items;

  g_assert (CHATTY_IS_MANAGER (self));

  model = G_LIST_MODEL (self->account_list);
  n_items = g_list_model_get_n_items (model);
  self->active_protocols = 0;

  for (guint i = 0; i < n_items; i++) {
    g_autoptr(ChattyAccount) account = NULL;

    account = g_list_model_get_item (model, i);
    status  = chatty_account_get_status (account);
    protocol = chatty_item_get_protocols (CHATTY_ITEM (account));

    if (status == CHATTY_CONNECTED)
      self->active_protocols |= protocol;
  }

  if (self->has_modem)
    self->active_protocols |= CHATTY_PROTOCOL_SMS;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE_PROTOCOLS]);
}

static void
manager_connection_signed_on_cb (PurpleConnection *gc,
                                 ChattyManager    *self)
{
  PurpleAccount *pp_account;
  ChattyPpAccount *account;
  ChattyProtocol protocol;

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

  protocol = chatty_item_get_protocols (CHATTY_ITEM (account));
  self->active_protocols |= protocol;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE_PROTOCOLS]);

  g_object_notify (G_OBJECT (account), "status");
}

static void
manager_connection_signed_off_cb (PurpleConnection *gc,
                                  ChattyManager    *self)
{
  PurpleAccount *pp_account;
  ChattyPpAccount *account;

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

  manager_update_protocols (self);

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

  purple_signal_connect (purple_conversations_get_handle (),
                         "conversation-created", self,
                         PURPLE_CALLBACK (manager_conversation_created_cb), self);
  purple_signal_connect (purple_conversations_get_handle (),
                         "chat-joined", self,
                         PURPLE_CALLBACK (manager_conversation_created_cb), self);
  purple_signal_connect (purple_conversations_get_handle (),
                         "conversation-updated", self,
                         PURPLE_CALLBACK (manager_conversation_updated_cb), self);
  purple_signal_connect (purple_conversations_get_handle (),
                         "deleting-conversation", self,
                         PURPLE_CALLBACK (manager_deleting_conversation_cb), self);
  purple_signal_connect (purple_conversations_get_handle(),
                         "wrote-im-msg", self,
                         PURPLE_CALLBACK (manager_wrote_chat_im_msg_cb), self);
  purple_signal_connect (chatty_conversations_get_handle (),
                         "wrote-chat-msg", self,
                         PURPLE_CALLBACK (manager_wrote_chat_im_msg_cb), self);

  purple_signal_connect (purple_conversations_get_handle (),
                         "chat-buddy-leaving", self,
                         PURPLE_CALLBACK (manager_conversation_buddy_leaving_cb), self);

  purple_signal_connect_priority (purple_connections_get_handle (),
                                  "autojoin", self,
                                  PURPLE_CALLBACK (manager_connection_autojoin_cb), self,
                                  PURPLE_SIGNAL_PRIORITY_HIGHEST);
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


  /**
   * ChattyManager::authorize-buddy:
   * @self: a #ChattyManager
   * @account: A #ChattyPpAccount
   * @remote_user: username of the remote user
   * @name: The Alias of @remote_user
   * @message: The message sent by @remote_user
   *
   * Emitted when some one requests to add them to the
   * @account’s buddy list.
   *
   * Returns: %GTK_RESPONSE_ACCEPT if authorized to be
   * added to buddy list, any other value means unauthorized.
   */
  signals [AUTHORIZE_BUDDY] =
    g_signal_new ("authorize-buddy",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_INT,
                  4, CHATTY_TYPE_PP_ACCOUNT, G_TYPE_STRING,
                  G_TYPE_STRING, G_TYPE_STRING);

  /**
   * ChattyManager::connection-error:
   * @self: a #ChattyManager
   * @account: A #ChattyPpAccount
   * @error: The error message
   *
   * Emitted when connection to @account failed
   */
  signals [CONNECTION_ERROR] =
    g_signal_new ("connection-error",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2, CHATTY_TYPE_PP_ACCOUNT, G_TYPE_STRING);

  /**
   * ChattyManager::notify-added:
   * @self: a #ChattyManager
   * @account: A #ChattyPpAccount
   * @remote_user: username of the remote user
   * @id: The ID for @remote_user
   *
   * Emitted when some buddy added @account username to their
   * buddy list.
   */
  signals [NOTIFY_ADDED] =
    g_signal_new ("notify-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  3, CHATTY_TYPE_PP_ACCOUNT, G_TYPE_STRING, G_TYPE_STRING);
}

static void
chatty_manager_init (ChattyManager *self)
{
  self->chatty_eds = chatty_eds_new (CHATTY_PROTOCOL_SMS);

  self->account_list = g_list_store_new (CHATTY_TYPE_PP_ACCOUNT);

  self->chat_list = g_list_store_new (CHATTY_TYPE_CHAT);
  self->im_list = g_list_store_new (CHATTY_TYPE_CHAT);
  self->list_of_chat_list = g_list_store_new (G_TYPE_LIST_MODEL);
  self->list_of_user_list = g_list_store_new (G_TYPE_LIST_MODEL);

  self->contact_list = gtk_flatten_list_model_new (G_TYPE_OBJECT,
                                                   G_LIST_MODEL (self->list_of_user_list));
  g_list_store_append (self->list_of_user_list, G_LIST_MODEL (self->chat_list));
  g_list_store_append (self->list_of_user_list,
                       chatty_eds_get_model (self->chatty_eds));

  self->chat_im_list = gtk_flatten_list_model_new (G_TYPE_OBJECT,
                                                   G_LIST_MODEL (self->list_of_chat_list));
  g_list_store_append (self->list_of_chat_list, G_LIST_MODEL (self->chat_list));
  g_list_store_append (self->list_of_chat_list, G_LIST_MODEL (self->im_list));

  self->chat_sorter = gtk_custom_sorter_new ((GCompareDataFunc)manager_sort_chat_item,
                                             NULL, NULL);
  self->sorted_chat_im_list = gtk_sort_list_model_new (G_LIST_MODEL (self->chat_im_list),
                                                       self->chat_sorter);

  g_signal_connect_object (self->chatty_eds, "notify::is-ready",
                           G_CALLBACK (manager_eds_is_ready), self,
                           G_CONNECT_SWAPPED);
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
  purple_accounts_set_ui_ops (&ui_ops);
  purple_request_set_ui_ops (chatty_request_get_ui_ops ());
  purple_notify_set_ui_ops (chatty_notify_get_ui_ops ());
  purple_blist_set_ui_ops (&blist_ui_ops);
  purple_conversations_set_ui_ops (chatty_conversations_get_conv_ui_ops ());
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


GListModel *
chatty_manager_get_chat_list (ChattyManager *self)
{
  g_return_val_if_fail (CHATTY_IS_MANAGER (self), NULL);

  return G_LIST_MODEL (self->sorted_chat_im_list);
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
  purple_signal_connect (purple_blist_get_handle (),
                         "buddy-privacy-changed", self,
                         PURPLE_CALLBACK (manager_buddy_privacy_chaged_cb), self);
  purple_signal_connect (purple_blist_get_handle (),
                         "buddy-signed-on", self,
                         PURPLE_CALLBACK (manager_buddy_signed_on_off_cb), self);
  purple_signal_connect (purple_blist_get_handle (),
                         "buddy-signed-off", self,
                         PURPLE_CALLBACK (manager_buddy_signed_on_off_cb), self);

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


ChattyEds *
chatty_manager_get_eds (ChattyManager *self)
{
  g_return_val_if_fail (CHATTY_IS_MANAGER (self), NULL);

  return self->chatty_eds;
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

static ChattyChat *
manager_find_im (GListModel         *model,
                 PurpleConversation *conv)
{
  guint n_items;

  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++) {
    g_autoptr(ChattyChat) chat = NULL;

    chat = g_list_model_get_item (model, i);

    if (chatty_chat_match_purple_conv (chat, conv))
      return chat;
  }

  return NULL;
}


ChattyChat *
chatty_manager_add_conversation (ChattyManager      *self,
                                 PurpleConversation *conv)
{
  ChattyChat *chat;

  g_return_val_if_fail (CHATTY_IS_MANAGER (self), NULL);

  if (!conv)
    return NULL;

  chat = manager_find_im (G_LIST_MODEL (self->chat_im_list), conv);

  if (chat) {
    chatty_chat_set_purple_conv (chat, conv);
    g_signal_emit_by_name (chat, "changed");

    return chat;
  }

  chat = chatty_chat_new_purple_conv (conv);
  g_list_store_append (self->im_list, chat);
  gtk_sorter_changed (self->chat_sorter, GTK_SORTER_ORDER_TOTAL);

  g_object_unref (chat);

  return chat;
}


void
chatty_manager_delete_conversation (ChattyManager      *self,
                                    PurpleConversation *conv)
{
  ChattyChat *chat;
  PurpleBuddy *pp_buddy;
  GListModel *model;

  g_return_if_fail (CHATTY_IS_MANAGER (self));
  g_return_if_fail (conv);

  model = G_LIST_MODEL (self->im_list);
  chat  = manager_find_im (model, conv);

  if (!chat) {
    model = G_LIST_MODEL (self->chat_list);
    chat  = manager_find_im (model, conv);
  }

  if (!chat)
    return;

  pp_buddy = chatty_chat_get_purple_buddy (chat);
  if (pp_buddy) {
    ChattyPpBuddy *buddy;

    buddy = chatty_pp_buddy_get_object (pp_buddy);

    if (buddy)
      chatty_pp_buddy_set_chat (buddy, NULL);
  }

  if (chat) {
    chatty_chat_view_remove_footer (CHATTY_CHAT_VIEW (CHATTY_CONVERSATION (conv)->chat_view));
    chatty_utils_remove_list_item (G_LIST_STORE (model), chat);
  }
}


static ChattyChat *
chatty_manager_find_chat (GListModel *model,
                          ChattyChat *item)
{
  guint n_items;

  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++) {
    g_autoptr(ChattyChat) chat = NULL;

    chat = g_list_model_get_item (model, i);

    if (chatty_chat_are_same (chat, item))
      return chat;
  }

  return NULL;
}


ChattyChat *
chatty_manager_add_chat (ChattyManager *self,
                         ChattyChat    *chat)
{
  ChattyChat *item;
  GListModel *model;

  g_return_val_if_fail (CHATTY_IS_MANAGER (self), NULL);
  g_return_val_if_fail (CHATTY_IS_CHAT (chat), NULL);

  model = G_LIST_MODEL (self->im_list);

  if (chatty_utils_get_item_position (model, chat, NULL))
    item = chat;
  else
    item = chatty_manager_find_chat (model, chat);

  if (!item)
    g_list_store_append (self->im_list, chat);

  gtk_sorter_changed (self->chat_sorter, GTK_SORTER_ORDER_TOTAL);

  return item ? item : chat;
}


ChattyChat *
chatty_manager_find_purple_conv (ChattyManager      *self,
                                 PurpleConversation *conv)
{
  g_return_val_if_fail (CHATTY_IS_MANAGER (self), NULL);

  return manager_find_im (G_LIST_MODEL (self->chat_im_list), conv);
}


gboolean
chatty_blist_protocol_is_sms (PurpleAccount *account)
{
  const gchar *protocol_id;

  g_return_val_if_fail (account != NULL, FALSE);

  protocol_id = purple_account_get_protocol_id (account);

  return g_strcmp0 (protocol_id, "purple-mm-sms") == 0;
}
