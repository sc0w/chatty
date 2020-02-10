/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-pp-buddy.c
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-pp-buddy"

#define _GNU_SOURCE
#include <string.h>
#include <purple.h>

#include "chatty-config.h"
#include "chatty-settings.h"
#include "chatty-account.h"
#include "chatty-window.h"
#include "chatty-purple-init.h"
#include "chatty-pp-buddy.h"

/**
 * SECTION: chatty-pp-buddy
 * @title: ChattyPpBuddy
 * @short_description: An abstraction over #PurpleBuddy
 * @include: "chatty-pp-buddy.h"
 */

struct _ChattyPpBuddy
{
  ChattyUser         parent_instance;

  char              *username;
  char              *name;
  PurpleAccount     *pp_account;
  PurpleBuddy       *pp_buddy;

  PurpleStoredImage *pp_avatar;
  GdkPixbuf         *avatar;
  ChattyProtocol     protocol;
};

G_DEFINE_TYPE (ChattyPpBuddy, chatty_pp_buddy, CHATTY_TYPE_USER)

enum {
  PROP_0,
  PROP_PURPLE_ACCOUNT,
  PROP_PURPLE_BUDDY,
  PROP_USERNAME,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
chatty_pp_buddy_update_protocol (ChattyPpBuddy *self)
{
  PurpleAccount *pp_account;
  ChattyUser *user;

  g_assert (CHATTY_IS_PP_BUDDY (self));
  g_assert (self->pp_buddy);

  pp_account = purple_buddy_get_account (self->pp_buddy);
  user = pp_account->ui_data;
  g_return_if_fail (user);

  self->protocol = chatty_user_get_protocols (user);
}

/* copied and modified from chatty_blist_add_buddy */
static void
chatty_add_new_buddy (ChattyPpBuddy *self)
{
  PurpleConversation *conv;
  const char *purple_id;

  g_assert (CHATTY_IS_PP_BUDDY (self));

  /* buddy should be NULL and account should be non-NULL */
  g_return_if_fail (!self->pp_buddy);
  g_return_if_fail (self->pp_account);

  self->pp_buddy = purple_buddy_new (self->pp_account,
                                     self->username,
                                     self->name);

  purple_blist_add_buddy (self->pp_buddy, NULL, NULL, NULL);

  g_debug ("%s: %s ", __func__, purple_buddy_get_name (self->pp_buddy));

  purple_id = purple_account_get_protocol_id (self->pp_account);

  if (g_strcmp0 (purple_id, "prpl-mm-sms") != 0)
    purple_account_add_buddy_with_invite (self->pp_account, self->pp_buddy, NULL);

  conv = purple_find_conversation_with_account (PURPLE_CONV_TYPE_IM,
                                                self->username,
                                                self->pp_account);

  if (conv != NULL)
    {
      PurpleBuddyIcon *icon;

      icon = purple_conv_im_get_icon (PURPLE_CONV_IM (conv));

      if (icon != NULL)
        purple_buddy_icon_update (icon);
    }
}

static ChattyProtocol
chatty_pp_buddy_get_protocols (ChattyUser *user)
{
  ChattyPpBuddy *self = (ChattyPpBuddy *)user;

  g_assert (CHATTY_IS_PP_BUDDY (self));

  if (self->protocol != CHATTY_PROTOCOL_NONE)
    return self->protocol;

  return CHATTY_USER_CLASS (chatty_pp_buddy_parent_class)->get_protocols (user);
}

static gboolean
chatty_pp_buddy_matches (ChattyUser     *user,
                         const char     *needle,
                         ChattyProtocol  protocols,
                         gboolean        match_name)
{
  ChattyPpBuddy *self = (ChattyPpBuddy *)user;
  const char *user_id;

  if (!self->pp_buddy)
    return FALSE;

  user_id = purple_buddy_get_name (self->pp_buddy);
  return strcasestr (user_id, needle) != NULL;
}

static const char *
chatty_pp_buddy_get_name (ChattyUser *user)
{
  ChattyPpBuddy *self = (ChattyPpBuddy *)user;
  PurpleContact *contact;
  const char *name;

  g_assert (CHATTY_IS_PP_BUDDY (self));

  if (!self->pp_buddy && self->name)
    return self->name;
  else if (!self->pp_buddy)
    return "";

  contact = purple_buddy_get_contact (self->pp_buddy);
  name = purple_contact_get_alias (contact);

  if (name && *name)
    return name;

  return purple_buddy_get_name (self->pp_buddy);
}

static void
chatty_pp_buddy_set_name (ChattyUser *user,
                          const char *name)
{
  ChattyPpBuddy *self = (ChattyPpBuddy *)user;
  PurpleContact *contact;

  g_assert (CHATTY_IS_PP_BUDDY (self));

  g_free (self->name);

  if (!name || !*name)
    self->name = NULL;
  else
    self->name = g_strdup (name);

  if (!self->pp_buddy)
    return;

  contact = purple_buddy_get_contact (self->pp_buddy);
  purple_blist_alias_contact (contact, self->name);
}

static void
chatty_pp_buddy_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  ChattyPpBuddy *self = (ChattyPpBuddy *)object;

  switch (prop_id)
    {
    case PROP_PURPLE_ACCOUNT:
      self->pp_account = g_value_get_pointer (value);
      break;

    case PROP_PURPLE_BUDDY:
      self->pp_buddy = g_value_get_pointer (value);
      break;

    case PROP_USERNAME:
      self->username = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
chatty_pp_buddy_constructed (GObject *object)
{
  ChattyPpBuddy *self = (ChattyPpBuddy *)object;
  ChattyBlistNode *chatty_node;
  PurpleBlistNode *node;

  G_OBJECT_CLASS (chatty_pp_buddy_parent_class)->constructed (object);

  if (!self->pp_buddy)
    chatty_add_new_buddy (self);

  chatty_pp_buddy_update_protocol (self);

  node = PURPLE_BLIST_NODE (self->pp_buddy);
  chatty_node = node->ui_data;

  chatty_node->buddy_object = self;
  g_object_add_weak_pointer (G_OBJECT (self), (gpointer *)&chatty_node->buddy_object);
}

static void
chatty_pp_buddy_finalize (GObject *object)
{
  ChattyPpBuddy *self = (ChattyPpBuddy *)object;

  g_free (self->username);
  g_free (self->name);

  G_OBJECT_CLASS (chatty_pp_buddy_parent_class)->finalize (object);
}

static void
chatty_pp_buddy_class_init (ChattyPpBuddyClass *klass)
{
  GObjectClass *object_class  = G_OBJECT_CLASS (klass);
  ChattyUserClass *user_class = CHATTY_USER_CLASS (klass);

  object_class->set_property = chatty_pp_buddy_set_property;
  object_class->constructed  = chatty_pp_buddy_constructed;
  object_class->finalize = chatty_pp_buddy_finalize;

  user_class->get_protocols = chatty_pp_buddy_get_protocols;
  user_class->matches  = chatty_pp_buddy_matches;
  user_class->get_name = chatty_pp_buddy_get_name;
  user_class->set_name = chatty_pp_buddy_set_name;

  properties[PROP_PURPLE_ACCOUNT] =
    g_param_spec_pointer ("purple-account",
                         "PurpleAccount",
                         "The PurpleAccount this buddy belongs to",
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_PURPLE_BUDDY] =
    g_param_spec_pointer ("purple-buddy",
                          "Purple PpBuddy",
                          "The PurpleBuddy to be used to create the object",
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_USERNAME] =
    g_param_spec_string ("username",
                         "Username",
                         "Username of the Purple buddy",
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
chatty_pp_buddy_init (ChattyPpBuddy *self)
{
}

ChattyPpBuddy *
chatty_pp_buddy_get_object (PurpleBuddy *buddy)
{
  PurpleBlistNode *node = PURPLE_BLIST_NODE (buddy);
  ChattyBlistNode *chatty_node;

  g_return_val_if_fail (PURPLE_BLIST_NODE_IS_BUDDY (node), NULL);

  chatty_node = node->ui_data;

  if (!chatty_node)
    return NULL;

  return chatty_node->buddy_object;
}

PurpleAccount *
chatty_pp_buddy_get_account (ChattyPpBuddy *self)
{
  g_return_val_if_fail (CHATTY_IS_PP_BUDDY (self), NULL);

  return purple_buddy_get_account (self->pp_buddy);
}

PurpleBuddy *
chatty_pp_buddy_get_buddy (ChattyPpBuddy *self)
{
  g_return_val_if_fail (CHATTY_IS_PP_BUDDY (self), NULL);

  return self->pp_buddy;
}
