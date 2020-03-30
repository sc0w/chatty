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
#include "chatty-icons.h"
#include "users/chatty-pp-buddy.h"
#include "chatty-chat.h"

#define CHATTY_COLOR_BLUE "4A8FD9"

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

  PurpleAccount      *account;
  PurpleBuddy        *buddy;

  PurpleChat         *pp_chat;
  PurpleConversation *conv;

  char               *last_message;
  guint               unread_count;
  guint               last_msg_time;
  e_msg_dir           last_msg_direction;
};

G_DEFINE_TYPE (ChattyChat, chatty_chat, CHATTY_TYPE_ITEM)

enum {
  PROP_0,
  PROP_PURPLE_CHAT,
  N_PROPS
};

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];
static GParamSpec *properties[N_PROPS];

static PurpleBlistNode *
chatty_get_conv_blist_node (PurpleConversation *conv)
{
  PurpleBlistNode *node = NULL;

  switch (purple_conversation_get_type (conv)) {
  case PURPLE_CONV_TYPE_IM:
    node = PURPLE_BLIST_NODE (purple_find_buddy (conv->account,
                                                 conv->name));
    break;
  case PURPLE_CONV_TYPE_CHAT:
    node = PURPLE_BLIST_NODE (purple_blist_find_chat (conv->account,
                                                      conv->name));
    break;
  case PURPLE_CONV_TYPE_UNKNOWN:
  case PURPLE_CONV_TYPE_MISC:
  case PURPLE_CONV_TYPE_ANY:
  default:
    g_warning ("Unhandled conversation type %d",
               purple_conversation_get_type (conv));
    break;
  }
  return node;
}

static const char *
chatty_chat_get_name (ChattyItem *item)
{
  ChattyChat *self = (ChattyChat *)item;
  const char *name = NULL;

  g_assert (CHATTY_IS_CHAT (self));

  if (self->pp_chat)
    name = purple_chat_get_name (self->pp_chat);
  else if (self->buddy)
    name = purple_buddy_get_alias_only (self->buddy);

  if (!name && self->buddy)
    name = purple_buddy_get_name (self->buddy);

  if (!name && self->conv)
    name = purple_conversation_get_title (self->conv);

  if (!name)
    name = "Invalid user";

  return name;
}


static ChattyProtocol
chatty_chat_get_protocols (ChattyItem *item)
{
  ChattyChat *self = (ChattyChat *)item;

  g_assert (CHATTY_IS_CHAT (self));

  return CHATTY_PROTOCOL_ANY;
}


static GdkPixbuf *
chatty_chat_get_avatar (ChattyItem *item)
{
  ChattyChat *self = (ChattyChat *)item;

  g_assert (CHATTY_IS_CHAT (self));

  if (self->buddy) {
    ChattyPpBuddy *buddy;

    buddy = chatty_pp_buddy_get_object (self->buddy);

    if (buddy)
      return chatty_item_get_avatar (CHATTY_ITEM (buddy));

    return NULL;
  }

  if (self->pp_chat)
    return chatty_icon_get_buddy_icon ((PurpleBlistNode *)self->pp_chat,
                                       NULL,
                                       CHATTY_ICON_SIZE_MEDIUM,
                                       CHATTY_COLOR_BLUE,
                                       FALSE);

  return NULL;
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
chatty_chat_finalize (GObject *object)
{
  ChattyChat *self = (ChattyChat *)object;

  g_free (self->last_message);

  G_OBJECT_CLASS (chatty_chat_parent_class)->finalize (object);
}


static void
chatty_chat_class_init (ChattyChatClass *klass)
{
  GObjectClass *object_class  = G_OBJECT_CLASS (klass);
  ChattyItemClass *item_class = CHATTY_ITEM_CLASS (klass);

  object_class->set_property = chatty_chat_set_property;
  object_class->finalize = chatty_chat_finalize;

  item_class->get_name = chatty_chat_get_name;
  item_class->get_protocols = chatty_chat_get_protocols;
  item_class->get_avatar = chatty_chat_get_avatar;

  properties[PROP_PURPLE_CHAT] =
    g_param_spec_pointer ("purple-chat",
                          "Purple Chat",
                          "The PurpleChat to be used to create the object",
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * ChattyChat::changed:
   * @self: a #ChattyChat
   *
   * Emitted when chat changes
   */
  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}


static void
chatty_chat_init (ChattyChat *self)
{
}


ChattyChat *
chatty_chat_new_im_chat (PurpleAccount *account,
                         PurpleBuddy   *buddy)
{
  ChattyChat *self;

  g_return_val_if_fail (account, NULL);
  g_return_val_if_fail (buddy, NULL);

  self = g_object_new (CHATTY_TYPE_CHAT, NULL);
  self->account = account;
  self->buddy = buddy;

  return self;
}


ChattyChat *
chatty_chat_new_purple_chat (PurpleChat *pp_chat)
{
  return g_object_new (CHATTY_TYPE_CHAT,
                       "purple-chat", pp_chat,
                       NULL);
}


ChattyChat *
chatty_chat_new_purple_conv (PurpleConversation *conv)
{
  ChattyChat *self;
  PurpleBlistNode *node;

  self = g_object_new (CHATTY_TYPE_CHAT, NULL);
  self->conv = conv;

  node = chatty_get_conv_blist_node (conv);

  if (node && PURPLE_BLIST_NODE_IS_CHAT (node))
    self->pp_chat = PURPLE_CHAT (node);
  else if (node && PURPLE_BLIST_NODE_IS_BUDDY (node))
    self->buddy = PURPLE_BUDDY (node);

  return self;
}


void
chatty_chat_set_purple_conv (ChattyChat         *self,
                             PurpleConversation *conv)
{
  PurpleBlistNode *node;

  g_return_if_fail (CHATTY_IS_CHAT (self));

  self->conv = conv;

  if (self->pp_chat || self->buddy)
    return;

  if (!conv)
    return;

  node = chatty_get_conv_blist_node (conv);

  if (node && PURPLE_BLIST_NODE_IS_CHAT (node))
    self->pp_chat = PURPLE_CHAT (node);
  else if (node && PURPLE_BLIST_NODE_IS_BUDDY (node))
    self->buddy = PURPLE_BUDDY (node);
}


PurpleChat *
chatty_chat_get_purple_chat (ChattyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (self), NULL);

  return self->pp_chat;
}


PurpleBuddy *
chatty_chat_get_purple_buddy (ChattyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (self), NULL);

  return self->buddy;
}


PurpleConversation *
chatty_chat_get_purple_conv (ChattyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (self), NULL);

  return self->conv;
}

const char *
chatty_chat_get_username (ChattyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (self), "");

  if (self->pp_chat)
    return purple_account_get_username (self->pp_chat->account);

  if (self->buddy)
    return purple_account_get_username (self->buddy->account);

  if (self->conv)
    return purple_account_get_username (self->conv->account);

  return "";
}

gboolean
chatty_chat_are_same (ChattyChat *a,
                      ChattyChat *b)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (a), FALSE);
  g_return_val_if_fail (CHATTY_IS_CHAT (b), FALSE);

  if (a == b)
    return TRUE;

  if (a->account && a->buddy &&
      a->account == b->account &&
      a->buddy == b->buddy)
    return TRUE;

  if (a->conv && a->conv == b->conv)
    return TRUE;

  if (a->pp_chat && a->pp_chat == b->pp_chat)
    return TRUE;

  if (a->conv &&
      chatty_chat_match_purple_conv (b, a->conv))
    return TRUE;

  if (b->conv &&
      chatty_chat_match_purple_conv (b, b->conv))
    return TRUE;

  return FALSE;
}

gboolean
chatty_chat_match_purple_conv (ChattyChat         *self,
                               PurpleConversation *conv)
{
  gpointer node;

  g_return_val_if_fail (CHATTY_IS_CHAT (self), FALSE);
  g_return_val_if_fail (conv, FALSE);

  if (self->conv && conv == self->conv)
    return TRUE;

  if (self->account && self->account != conv->account)
    return FALSE;

  node = chatty_get_conv_blist_node (conv);

  if (!node)
    return FALSE;

  if (node == self->pp_chat ||
      node == self->buddy) {
    self->conv = conv;

    return TRUE;
  }

  return FALSE;
}


const char *
chatty_chat_get_last_message (ChattyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (self), "");

  if (self->last_message)
    return self->last_message;

  return "";
}


void
chatty_chat_set_last_message (ChattyChat *self,
                              const char *message)
{
  g_return_if_fail (CHATTY_IS_CHAT (self));

  g_free (self->last_message);
  self->last_message = g_strdup (message);

  g_signal_emit (self, signals[CHANGED], 0);
}

guint
chatty_chat_get_unread_count (ChattyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (self), 0);

  return self->unread_count;
}

void
chatty_chat_set_unread_count (ChattyChat *self,
                              guint       unread_count)
{
  g_return_if_fail (CHATTY_IS_CHAT (self));

  if (self->unread_count == unread_count)
    return;

  self->unread_count = unread_count;
  g_signal_emit (self, signals[CHANGED], 0);
}

e_msg_dir
chatty_chat_get_last_msg_direction (ChattyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (self), MSG_IS_SYSTEM);

  return self->last_msg_direction;
}

void
chatty_chat_set_last_msg_direction (ChattyChat *self,
                                    e_msg_dir   direction)
{
  g_return_if_fail (CHATTY_IS_CHAT (self));

  if (self->last_msg_direction == direction)
    return;

  self->last_msg_direction = direction;
  g_signal_emit (self, signals[CHANGED], 0);
}

time_t
chatty_chat_get_last_msg_time (ChattyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (self), 0);

  return self->last_msg_time;
}

void
chatty_chat_set_last_msg_time (ChattyChat *self,
                               time_t      msg_time)
{
  g_return_if_fail (CHATTY_IS_CHAT (self));

  if (self->last_msg_time == msg_time)
    return;

  self->last_msg_time = msg_time;
  g_signal_emit (self, signals[CHANGED], 0);
}
