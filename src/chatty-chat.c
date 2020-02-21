/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-chat.c
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-chat"

#include <purple.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "chatty-settings.h"
#include "chatty-chat.h"

/**
 * SECTION: chatty-chat
 * @title: ChattyChat
 * @short_description: An abstraction over #PurpleConversation
 * @include: "chatty-chat.h"
 *
 * libpurple doesn’t have a nice OOP interface for managing anthing.
 * This class hides all the complexities surrounding it.
 */

struct _ChattyChat
{
  ChattyItem          parent_instance;

  PurpleChat         *pp_chat;
};

G_DEFINE_TYPE (ChattyChat, chatty_chat, CHATTY_TYPE_ITEM)

enum {
  PROP_0,
  PROP_PURPLE_CHAT,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static const char *
chatty_chat_get_name (ChattyItem *item)
{
  ChattyChat *self = (ChattyChat *)item;
  const char *name;

  g_assert (CHATTY_IS_CHAT (self));

  name = purple_chat_get_name (self->pp_chat);

  if (!name)
    name = "";

  return name;
}


static ChattyProtocol
chatty_chat_get_protocols (ChattyItem *item)
{
  ChattyChat *self = (ChattyChat *)item;

  g_assert (CHATTY_IS_CHAT (self));

  return CHATTY_PROTOCOL_ANY;
}


static void
chatty_chat_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  ChattyChat *self = (ChattyChat *)object;

  switch (prop_id)
    {
    case PROP_PURPLE_CHAT:
      self->pp_chat = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
chatty_chat_class_init (ChattyChatClass *klass)
{
  GObjectClass *object_class  = G_OBJECT_CLASS (klass);
  ChattyItemClass *item_class = CHATTY_ITEM_CLASS (klass);

  object_class->set_property = chatty_chat_set_property;

  item_class->get_name = chatty_chat_get_name;
  item_class->get_protocols = chatty_chat_get_protocols;

  properties[PROP_PURPLE_CHAT] =
    g_param_spec_pointer ("purple-chat",
                          "Purple Chat",
                          "The PurpleChat to be used to create the object",
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}


static void
chatty_chat_init (ChattyChat *self)
{
}


ChattyChat *
chatty_chat_new_purple_chat (PurpleChat *pp_chat)
{
  return g_object_new (CHATTY_TYPE_CHAT,
                       "purple-chat", pp_chat,
                       NULL);
}


PurpleChat *
chatty_chat_get_purple_chat (ChattyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (self), NULL);

  return self->pp_chat;
}
