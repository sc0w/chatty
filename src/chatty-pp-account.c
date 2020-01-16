/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-pp-account.c
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-pp-account"

#include <purple.h>

#include "chatty-config.h"
#include "chatty-settings.h"
#include "chatty-account.h"
#include "chatty-window.h"
#include "chatty-purple-init.h"
#include "chatty-pp-account.h"

/**
 * SECTION: chatty-pp-account
 * @title: ChattyPpAccount
 * @short_description: An abstraction over #PurpleAccount
 * @include: "chatty-pp-account.h"
 *
 * libpurple doesn’t have a nice OOP interface for managing anthing.
 * This class hides all the complexities surrounding it.
 */

#define RECONNECT_DELAY 5000 /* milliseconds */

struct _ChattyPpAccount
{
  GObject         parent_instance;

  gchar          *username;
  gchar          *protocol_id;

  PurpleAccount  *pp_account;
  guint           connect_id;
};

G_DEFINE_TYPE (ChattyPpAccount, chatty_pp_account, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_USERNAME,
  PROP_PROTOCOL_ID,
  PROP_PURPLE_ACCOUNT,
  PROP_ENABLED,
  PROP_STATUS,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static gboolean
account_connect (ChattyPpAccount *self)
{
  g_assert (CHATTY_IS_PP_ACCOUNT (self));

  g_clear_handle_id (&self->connect_id, g_source_remove);

  chatty_pp_account_connect (self, FALSE);

  return G_SOURCE_REMOVE;
}

static void
chatty_pp_account_get_property (GObject *object,
                                guint    prop_id,
                                GValue  *value,
                                GParamSpec *pspec)
{
  ChattyPpAccount *self = (ChattyPpAccount *)object;

  switch (prop_id)
    {
    case PROP_ENABLED:
      g_value_set_boolean (value, chatty_pp_account_get_enabled (self));
      break;

    case PROP_STATUS:
      g_value_set_int (value, chatty_pp_account_get_status (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
chatty_pp_account_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ChattyPpAccount *self = (ChattyPpAccount *)object;

  switch (prop_id)
    {
    case PROP_USERNAME:
      self->username = g_value_dup_string (value);
      break;

    case PROP_PROTOCOL_ID:
      self->protocol_id = g_value_dup_string (value);
      break;

    case PROP_PURPLE_ACCOUNT:
      self->pp_account = g_value_get_pointer (value);
      break;

    case PROP_ENABLED:
      chatty_pp_account_set_enabled (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
chatty_pp_account_constructed (GObject *object)
{
  ChattyPpAccount *self = (ChattyPpAccount *)object;

  G_OBJECT_CLASS (chatty_pp_account_parent_class)->constructed (object);

  if (!self->pp_account)
    self->pp_account = purple_account_new (self->username, self->protocol_id);
}

static void
chatty_pp_account_finalize (GObject *object)
{
  ChattyPpAccount *self = (ChattyPpAccount *)object;

  g_clear_handle_id (&self->connect_id, g_source_remove);
  g_free (self->username);
  g_free (self->protocol_id);

  G_OBJECT_CLASS (chatty_pp_account_parent_class)->finalize (object);
}

static void
chatty_pp_account_class_init (ChattyPpAccountClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = chatty_pp_account_get_property;
  object_class->set_property = chatty_pp_account_set_property;
  object_class->constructed = chatty_pp_account_constructed;
  object_class->finalize = chatty_pp_account_finalize;

  properties[PROP_USERNAME] =
    g_param_spec_string ("username",
                         "Username",
                         "Username of the Purple Account",
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_PROTOCOL_ID] =
    g_param_spec_string ("protocol-id",
                         "Protocol ID",
                         "the Protocol ID of the Purple account",
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_PURPLE_ACCOUNT] =
    g_param_spec_pointer ("purple-account",
                         "Purple Account",
                         "The PurpleAccount to be used to create the object",
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_ENABLED] =
    g_param_spec_boolean ("enabled",
                          "Enabled",
                          "Account Enabled or not",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_STATUS] =
    g_param_spec_int ("status",
                      "Status",
                      "Account connection status",
                      CHATTY_DISCONNECTED,
                      CHATTY_CONNECTED,
                      CHATTY_DISCONNECTED,
                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
chatty_pp_account_init (ChattyPpAccount *self)
{
}


ChattyPpAccount *
chatty_pp_account_new (const char *username,
                       const char *protocol_id)
{
  g_return_val_if_fail (username || *username, NULL);
  g_return_val_if_fail (protocol_id || *protocol_id, NULL);

  return g_object_new (CHATTY_TYPE_PP_ACCOUNT,
                       "username", username,
                       "protocol-id", protocol_id,
                       NULL);
}

ChattyPpAccount *
chatty_pp_account_new_purple (PurpleAccount *account)
{
  g_return_val_if_fail (account, NULL);

  return g_object_new (CHATTY_TYPE_PP_ACCOUNT,
                       "purple-account", account,
                       NULL);
}

/**
 * chatty_pp_account_save:
 * @self: A #ChattyPpAccount
 *
 * Save @self to accounts store, which is saved to disk.
 * If the account is already saved, the function simply
 * returns.
 */
void
chatty_pp_account_save (ChattyPpAccount *self)
{
  g_return_if_fail (CHATTY_IS_PP_ACCOUNT (self));

  /* purple adds the account only if not yet added */
  purple_accounts_add (self->pp_account);
}

/* XXX: a helper API till the dust settles */
PurpleAccount *
chatty_pp_account_get_account (ChattyPpAccount *self)
{
  g_return_val_if_fail (CHATTY_IS_PP_ACCOUNT (self), NULL);

  return self->pp_account;
}

/* XXX: a helper API till the dust settles */
PurpleStatus *
chatty_pp_account_get_active_status (ChattyPpAccount *self)
{
  g_return_val_if_fail (CHATTY_IS_PP_ACCOUNT (self), NULL);

  return purple_account_get_active_status (self->pp_account);
}

ChattyStatus
chatty_pp_account_get_status (ChattyPpAccount *self)
{
  g_return_val_if_fail (CHATTY_IS_PP_ACCOUNT (self), CHATTY_DISCONNECTED);

  if (purple_account_is_connected (self->pp_account))
    return CHATTY_CONNECTED;
  if (purple_account_is_connecting (self->pp_account))
    return CHATTY_CONNECTING;

  return CHATTY_DISCONNECTED;
}

gboolean
chatty_pp_account_is_sms (ChattyPpAccount *self)
{
  const char *id;

  g_return_val_if_fail (CHATTY_IS_PP_ACCOUNT (self), FALSE);

  id = chatty_pp_account_get_protocol_id (self);

  return g_str_equal (id, "prpl-mm-sms");
}

const char *
chatty_pp_account_get_protocol_id (ChattyPpAccount *self)
{
  const char *id;

  g_return_val_if_fail (CHATTY_IS_PP_ACCOUNT (self), "");

  id = purple_account_get_protocol_id (self->pp_account);

  return id ? id : "";
}

const char *
chatty_pp_account_get_protocol_name (ChattyPpAccount *self)
{
  g_return_val_if_fail (CHATTY_IS_PP_ACCOUNT (self), NULL);

  return purple_account_get_protocol_name (self->pp_account);
}

void
chatty_pp_account_set_enabled (ChattyPpAccount *self,
                               gboolean         enable)
{
  g_return_if_fail (CHATTY_IS_PP_ACCOUNT (self));

  purple_account_set_enabled (self->pp_account, CHATTY_UI, !!enable);
}

gboolean
chatty_pp_account_get_enabled (ChattyPpAccount *self)
{
  g_return_val_if_fail (CHATTY_IS_PP_ACCOUNT (self), FALSE);

  return purple_account_get_enabled (self->pp_account, CHATTY_UI);
}

void
chatty_pp_account_set_username (ChattyPpAccount *self,
                                const char      *username)
{
  g_return_if_fail (CHATTY_IS_PP_ACCOUNT (self));

  purple_account_set_username (self->pp_account, username);
}

const gchar *
chatty_pp_account_get_username (ChattyPpAccount *self)
{
  g_return_val_if_fail (CHATTY_IS_PP_ACCOUNT (self), NULL);

  return purple_account_get_username (self->pp_account);
}

void
chatty_pp_account_set_password (ChattyPpAccount *self,
                                const char      *password)
{
  const char *id;

  g_return_if_fail (CHATTY_IS_PP_ACCOUNT (self));

  id = chatty_pp_account_get_protocol_id (self);

  if (g_str_equal (id, "prpl-telegram"))
    purple_account_set_string (self->pp_account, "password-two-factor", password);
  else
    purple_account_set_password (self->pp_account, password);
}

const char *
chatty_pp_account_get_password (ChattyPpAccount *self)
{
  g_return_val_if_fail (CHATTY_IS_PP_ACCOUNT (self), NULL);

  return purple_account_get_password (self->pp_account);
}

void
chatty_pp_account_set_remember_password (ChattyPpAccount *self,
                                         gboolean         remember)
{
  g_return_if_fail (CHATTY_IS_PP_ACCOUNT (self));

  purple_account_set_remember_password (self->pp_account, !!remember);
}

gboolean
chatty_pp_account_get_remember_password (ChattyPpAccount *self)
{
  g_return_val_if_fail (CHATTY_IS_PP_ACCOUNT (self), FALSE);

  return purple_account_get_remember_password (self->pp_account);
}

/**
 * chatty_pp_account_connect:
 * @self: A #ChattyPpAccount
 * @delay: Whether to delay connection
 *
 * connection to @self.  If @delay is %TRUE, the connection
 * is initiated after some delay, which can be useful when
 * trying to connect after a connection failure.
 *
 * If the account is not enabled, or if account status is
 * set to offline, or if already connected, the function
 * simply returns.
 */
void
chatty_pp_account_connect (ChattyPpAccount *self,
                           gboolean         delay)
{
  PurpleStatus *pp_status;
  ChattyStatus status;

  g_return_if_fail (CHATTY_IS_PP_ACCOUNT (self));

  g_clear_handle_id (&self->connect_id, g_source_remove);

  if (!chatty_pp_account_get_enabled (self))
    return;

  status = chatty_pp_account_get_status (self);

  if (status != CHATTY_DISCONNECTED)
    return;

  pp_status = chatty_pp_account_get_active_status (self);

  if (!purple_status_is_online (pp_status))
    return;

  if (!delay)
    purple_account_connect (self->pp_account);
  else
    self->connect_id = g_timeout_add (RECONNECT_DELAY,
                                      G_SOURCE_FUNC (account_connect),
                                      self);
}

void
chatty_pp_account_disconnect (ChattyPpAccount *self)
{
  g_autofree char *password = NULL;
  ChattyStatus status;

  g_return_if_fail (CHATTY_IS_PP_ACCOUNT (self));

  if (chatty_pp_account_is_sms (self))
    return;

  status = chatty_pp_account_get_status (self);

  if (status == CHATTY_DISCONNECTED)
    return;

  password = g_strdup (chatty_pp_account_get_password (self));
  purple_account_disconnect (self->pp_account);
  chatty_pp_account_set_password (self, password);
}
