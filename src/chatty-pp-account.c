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

struct _ChattyPpAccount
{
  GObject         parent_instance;

  gchar          *username;
  gchar          *protocol_id;

  PurpleAccount  *pp_account;
};

G_DEFINE_TYPE (ChattyPpAccount, chatty_pp_account, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_USERNAME,
  PROP_PROTOCOL_ID,
  PROP_PURPLE_ACCOUNT,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

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

  g_free (self->username);
  g_free (self->protocol_id);

  G_OBJECT_CLASS (chatty_pp_account_parent_class)->finalize (object);
}

static void
chatty_pp_account_class_init (ChattyPpAccountClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

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

gboolean
chatty_pp_account_is_disconnected (ChattyPpAccount *self)
{
  g_return_val_if_fail (CHATTY_IS_PP_ACCOUNT (self), TRUE);

  return purple_account_is_disconnected (self->pp_account);
}

gboolean
chatty_pp_account_is_connected (ChattyPpAccount *self)
{
  g_return_val_if_fail (CHATTY_IS_PP_ACCOUNT (self), FALSE);

  return purple_account_is_connected (self->pp_account);
}

gboolean
chatty_pp_account_is_connecting (ChattyPpAccount *self)
{
  g_return_val_if_fail (CHATTY_IS_PP_ACCOUNT (self), FALSE);

  return purple_account_is_connecting (self->pp_account);
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
