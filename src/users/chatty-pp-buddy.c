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
#include "chatty-pp-account.h"
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
  ChattyItem         parent_instance;

  char              *username;
  char              *name;
  PurpleAccount     *pp_account;
  PurpleBuddy       *pp_buddy;
  ChattyContact     *contact;

  PurpleStoredImage *pp_avatar;
  GdkPixbuf         *avatar;
  ChattyProtocol     protocol;
};

G_DEFINE_TYPE (ChattyPpBuddy, chatty_pp_buddy, CHATTY_TYPE_ITEM)

enum {
  PROP_0,
  PROP_PURPLE_ACCOUNT,
  PROP_PURPLE_BUDDY,
  PROP_USERNAME,
  N_PROPS
};

enum {
  CHANGED,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

static void
chatty_pp_buddy_update_protocol (ChattyPpBuddy *self)
{
  PurpleAccount *pp_account;
  ChattyItem *item;

  g_assert (CHATTY_IS_PP_BUDDY (self));
  g_assert (self->pp_buddy);

  pp_account = purple_buddy_get_account (self->pp_buddy);
  item = pp_account->ui_data;
  g_return_if_fail (item);

  self->protocol = chatty_item_get_protocols (item);
}


static GdkPixbuf *
chatty_icon_from_data (const guchar *buf,
                       gsize         size)
{
  g_autoptr(GdkPixbufLoader) loader = NULL;
  g_autoptr(GError) error = NULL;
  GdkPixbuf *pixbuf = NULL;

  loader = gdk_pixbuf_loader_new ();
  gdk_pixbuf_loader_write (loader, buf, size, &error);

  if (!error)
    gdk_pixbuf_loader_close (loader, &error);

  if (error)
    g_warning ("Error: %s: %s", __func__, error->message);
  else
    pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

  if (!pixbuf)
    g_warning ("%s: pixbuf creation failed", __func__);

  return g_object_ref (pixbuf);
}


/* copied and modified from chatty_blist_add_buddy */
static void
chatty_add_new_buddy (ChattyPpBuddy *self)
{
  PurpleConversation *conv;
  const char *purple_id;

  g_assert (CHATTY_IS_PP_BUDDY (self));

  /* buddy should be NULL and account should be non-NULL */
  g_return_if_fail (self->pp_account);

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
chatty_pp_buddy_get_protocols (ChattyItem *item)
{
  ChattyPpBuddy *self = (ChattyPpBuddy *)item;

  g_assert (CHATTY_IS_PP_BUDDY (self));

  if (self->protocol != CHATTY_PROTOCOL_NONE)
    return self->protocol;

  return CHATTY_ITEM_CLASS (chatty_pp_buddy_parent_class)->get_protocols (item);
}

static gboolean
chatty_pp_buddy_matches (ChattyItem     *item,
                         const char     *needle,
                         ChattyProtocol  protocols,
                         gboolean        match_name)
{
  ChattyPpBuddy *self = (ChattyPpBuddy *)item;
  const char *item_id;

  if (!self->pp_buddy)
    return FALSE;

  item_id = purple_buddy_get_name (self->pp_buddy);
  return strcasestr (item_id, needle) != NULL;
}

static const char *
chatty_pp_buddy_get_name (ChattyItem *item)
{
  ChattyPpBuddy *self = (ChattyPpBuddy *)item;
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
chatty_pp_buddy_set_name (ChattyItem *item,
                          const char *name)
{
  ChattyPpBuddy *self = (ChattyPpBuddy *)item;
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


static GdkPixbuf *
chatty_pp_buddy_get_avatar (ChattyItem *item)
{
  ChattyPpBuddy *self = (ChattyPpBuddy *)item;
  PurpleContact *contact;
  PurpleStoredImage *img;
  PurpleBuddyIcon *icon = NULL;
  gconstpointer data = NULL;
  size_t len;

  g_assert (CHATTY_IS_PP_BUDDY (self));

  contact = purple_buddy_get_contact (self->pp_buddy);
  img = purple_buddy_icons_node_find_custom_icon ((PurpleBlistNode*)contact);

  if (!img) {
    icon = purple_buddy_get_icon (self->pp_buddy);

    if (!icon)
      icon = purple_buddy_icons_find (self->pp_buddy->account, self->pp_buddy->name);

    if (icon)
      data = purple_buddy_icon_get_data (icon, &len);

    if (!data)
      return NULL;
  }

  if (!data) {
    data = purple_imgstore_get_data (img);
    len = purple_imgstore_get_size (img);
  }

  return chatty_icon_from_data (data, len);
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
  ChattyPpAccount *account;
  GListModel *model;
  gboolean has_pp_buddy;

  G_OBJECT_CLASS (chatty_pp_buddy_parent_class)->constructed (object);

  has_pp_buddy = self->pp_buddy != NULL;

  if (!has_pp_buddy)
    self->pp_buddy = purple_buddy_new (self->pp_account,
                                       self->username,
                                       self->name);

  account = self->pp_buddy->account->ui_data;
  model = chatty_pp_account_get_buddy_list (account);
  g_list_store_append (G_LIST_STORE (model), self);
  chatty_pp_buddy_update_protocol (self);

  if (!has_pp_buddy)
    chatty_add_new_buddy (self);
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
  ChattyItemClass *item_class = CHATTY_ITEM_CLASS (klass);

  object_class->set_property = chatty_pp_buddy_set_property;
  object_class->constructed  = chatty_pp_buddy_constructed;
  object_class->finalize = chatty_pp_buddy_finalize;

  item_class->get_protocols = chatty_pp_buddy_get_protocols;
  item_class->matches  = chatty_pp_buddy_matches;
  item_class->get_name = chatty_pp_buddy_get_name;
  item_class->set_name = chatty_pp_buddy_set_name;
  item_class->get_avatar = chatty_pp_buddy_get_avatar;

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

  /**
   * ChattyPpBuddy::changed:
   * @self: a #ChattyPpBuddy
   *
   * changed signal is emitted when any detail
   * of the buddy changes.
   */
  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
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


/**
 * chatty_pp_buddy_get_id:
 * @self: a #ChattyPpBuddy
 *
 * Get the user id of @self.  For XMPP, this
 * is the XMPP name (eg: name@example.com).
 * For SMS buddy, this is the phone number.
 *
 * Returns: (transfer none): the id of Buddy.
 * or an empty string if not found or on error.
 */
const char *
chatty_pp_buddy_get_id (ChattyPpBuddy *self)
{
  const char *name;

  g_return_val_if_fail (CHATTY_IS_PP_BUDDY (self), "");

  if (!self->pp_buddy)
    return "";

  name = purple_buddy_get_name (self->pp_buddy);

  if (!name)
    name = "";

  return name;
}

ChattyContact *
chatty_pp_buddy_get_contact (ChattyPpBuddy *self)
{
  g_return_val_if_fail (CHATTY_IS_PP_BUDDY (self), NULL);

  return self->contact;
}


void
chatty_pp_buddy_set_contact (ChattyPpBuddy *self,
                             ChattyContact *contact)
{
  g_return_if_fail (CHATTY_IS_PP_BUDDY (self));
  g_return_if_fail (!contact || CHATTY_IS_CONTACT (contact));

  g_set_object (&self->contact, contact);
}
