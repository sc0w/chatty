/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-user.c
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-user"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "chatty-user.h"

/**
 * SECTION: chatty-user
 * @title: ChattyUser
 * @short_description: The base class for Accounts, Buddies and Contacts
 */

typedef struct
{
  gint dummy;
} ChattyUserPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ChattyUser, chatty_user, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_NAME,
  N_PROPS
};

enum {
  AVATAR_CHANGED,
  DELETED,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

static const char *
chatty_user_real_get_name (ChattyUser *self)
{
  g_assert (CHATTY_IS_USER (self));

  return "";
}

static void
chatty_user_real_set_name (ChattyUser *self,
                           const char *name)
{
  g_assert (CHATTY_IS_USER (self));

  /* Do Nothing */
}

static GdkPixbuf *
chatty_user_real_get_avatar (ChattyUser *self)
{
  g_assert (CHATTY_IS_USER (self));

  return NULL;
}

static void
chatty_user_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  ChattyUser *self = (ChattyUser *)object;

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, chatty_user_get_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
chatty_user_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  ChattyUser *self = (ChattyUser *)object;

  switch (prop_id)
    {
    case PROP_NAME:
      chatty_user_set_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
chatty_user_class_init (ChattyUserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = chatty_user_get_property;
  object_class->set_property = chatty_user_set_property;

  klass->get_name = chatty_user_real_get_name;
  klass->set_name = chatty_user_real_set_name;
  klass->get_avatar = chatty_user_real_get_avatar;

  /**
   * ChattyUser:name:
   *
   * The name of the User.
   */
  properties[PROP_NAME] =
    g_param_spec_string ("name",
                         "name",
                         "The name of the User",
                         "",
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * ChattyUser::avatar-changed:
   * @self: a #ChattyUser
   *
   * avatar-changed signal is emitted when the user’s
   * avatar change.
   */
  signals [AVATAR_CHANGED] =
    g_signal_new ("avatar-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * ChattyUser::deleted:
   * @self: a #ChattyUser
   *
   * deleted signal is emitted when the account
   * is deleted
   */
  signals [DELETED] =
    g_signal_new ("deleted",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
chatty_user_init (ChattyUser *self)
{
}

/**
 * chatty_user_get_name:
 * @self: a #ChattyUser
 *
 * Get the name of User.  In purple/pidgin it’s
 * termed as ‘alias.’
 *
 * Returns: (transfer none): the name of User.
 */
const char *
chatty_user_get_name (ChattyUser *self)
{
  g_return_val_if_fail (CHATTY_IS_USER (self), "");

  return CHATTY_USER_GET_CLASS (self)->get_name (self);
}

/**
 * chatty_user_set_name:
 * @self: a #ChattyUser
 * @name: (nullable): a text to set as name
 *
 * Set the name of User. Can be %NULL.
 */
void
chatty_user_set_name (ChattyUser *self,
                      const char *name)
{
  g_return_if_fail (CHATTY_IS_USER (self));

  CHATTY_USER_GET_CLASS (self)->set_name (self, name);
}

GdkPixbuf *
chatty_user_get_avatar (ChattyUser *self)
{
  g_return_val_if_fail (CHATTY_IS_USER (self), NULL);

  return CHATTY_USER_GET_CLASS (self)->get_avatar (self);
}
