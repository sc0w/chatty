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

#include "chatty-settings.h"
#include "chatty-account.h"
#include "chatty-utils.h"
#include "chatty-window.h"
#include "chatty-pp-account.h"
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

  GListStore      *account_list;
  gboolean         disable_auto_login;
  gboolean         network_available;
};

G_DEFINE_TYPE (ChattyManager, chatty_manager, G_TYPE_OBJECT)


static void
manager_account_added_cb (PurpleAccount *pp_account,
                          ChattyManager *self)
{
  g_autoptr(ChattyPpAccount) account = NULL;

  g_assert (CHATTY_IS_MANAGER (self));
  g_return_if_fail (!chatty_pp_account_find (pp_account));

  account = chatty_pp_account_new_purple (pp_account);
  g_object_notify (G_OBJECT (account), "status");
  g_list_store_append (self->account_list, account);

  if (self->disable_auto_login)
    chatty_pp_account_set_enabled (account, FALSE);

  if (chatty_pp_account_is_sms (account))
    chatty_pp_account_set_enabled (account, TRUE);
}

static void
manager_account_removed_cb (PurpleAccount *pp_account,
                            ChattyManager *self)
{
  ChattyPpAccount *account;

  g_assert (CHATTY_IS_MANAGER (self));

  /* account should exist in the store */
  account = chatty_pp_account_find (pp_account);
  g_return_if_fail (account);

  g_object_notify (G_OBJECT (account), "status");
  chatty_pp_account_remove (account);
}

static void
manager_account_changed_cb (PurpleAccount *pp_account,
                            ChattyManager *self)
{
  ChattyPpAccount *account;

  account = chatty_pp_account_find (pp_account);
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
  account = chatty_pp_account_find (pp_account);
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
  account = chatty_pp_account_find (pp_account);

  if (account)
    g_object_notify (G_OBJECT (account), "status");
  else
    g_return_if_reached ();
}

static void
manager_sms_modem_added_cb (gint status)
{
  ChattyPpAccount *account;
  PurpleAccount   *pp_account;

  pp_account = purple_accounts_find ("SMS", "prpl-mm-sms");
  account = chatty_pp_account_find (pp_account);
  g_return_if_fail (CHATTY_IS_PP_ACCOUNT (account));

  chatty_pp_account_connect (account, TRUE);
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
                         PURPLE_CALLBACK (manager_connection_changed_cb), self);
  purple_signal_connect (purple_connections_get_handle(),
                         "signed-off", self,
                         PURPLE_CALLBACK (manager_connection_changed_cb), self);

  purple_signal_connect (purple_plugins_get_handle (),
                         "mm-sms-modem-added", self,
                         PURPLE_CALLBACK (manager_sms_modem_added_cb), NULL);

  g_signal_connect_object (network_monitor, "network-changed",
                           G_CALLBACK (manager_network_changed_cb), self,
                           G_CONNECT_AFTER);
}

static void
chatty_manager_dispose (GObject *object)
{
  ChattyManager *self = (ChattyManager *)object;

  purple_signals_disconnect_by_handle (self);
  g_clear_object (&self->account_list);

  G_OBJECT_CLASS (chatty_manager_parent_class)->dispose (object);
}

static void
chatty_manager_class_init (ChattyManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = chatty_manager_dispose;
}

static void
chatty_manager_init (ChattyManager *self)
{
  chatty_data_t *chatty = chatty_get_data ();

  self->account_list = g_object_ref (chatty->account_list);
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

void
chatty_manager_enable_sms_account (ChattyManager *self)
{
  g_autoptr(ChattyPpAccount) account = NULL;
  PurpleAccount *pp_account;

  pp_account = purple_accounts_find ("SMS", "prpl-mm-sms");

  /* SMS plugin already initialized */
  if (pp_account)
    return;

  account = chatty_pp_account_new ("SMS", "prpl-mm-sms");
  chatty_pp_account_set_password (account, NULL);
  chatty_pp_account_set_remember_password (account, TRUE);
  chatty_pp_account_save (account);
}

GListModel *
chatty_manager_get_accounts (ChattyManager *self)
{
  g_return_val_if_fail (CHATTY_IS_MANAGER (self), NULL);

  return G_LIST_MODEL (self->account_list);
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
