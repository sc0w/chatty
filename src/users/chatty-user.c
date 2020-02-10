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

#define _GNU_SOURCE
#include <string.h>

#include "chatty-user.h"

/**
 * SECTION: chatty-user
 * @title: ChattyUser
 * @short_description: The base class for Accounts, Buddies and Contacts
 */

typedef struct
{
  ChattyProtocol protocols;
} ChattyUserPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ChattyUser, chatty_user, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_PROTOCOLS,
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

static ChattyProtocol
chatty_user_real_get_protocols (ChattyUser *self)
{
  ChattyUserPrivate *priv = chatty_user_get_instance_private (self);

  g_assert (CHATTY_IS_USER (self));

  return priv->protocols;
}

static gboolean
chatty_user_real_matches (ChattyUser     *self,
                          const char     *needle,
                          ChattyProtocol  protocols,
                          gboolean        match_name)
{
  const char *name;

  g_assert (CHATTY_IS_USER (self));

  name = chatty_user_get_name (self);

  return strcasestr (name, needle) != NULL;
}

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
chatty_user_real_set_avatar_async (ChattyUser          *self,
                                   const char          *filename,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_assert (CHATTY_IS_USER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (self, callback, user_data,
                           chatty_user_real_set_avatar_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Setting Custom avatar not supported");
}

static gboolean
chatty_user_real_set_avatar_finish (ChattyUser    *self,
                                    GAsyncResult  *result,
                                    GError       **error)
{
  g_assert (CHATTY_IS_USER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
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
    case PROP_PROTOCOLS:
      g_value_set_int (value, chatty_user_get_protocols (self));
      break;

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
  ChattyUserPrivate *priv = chatty_user_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PROTOCOLS:
      priv->protocols = g_value_get_int (value);
      break;

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

  klass->get_protocols = chatty_user_real_get_protocols;
  klass->matches  = chatty_user_real_matches;
  klass->get_name = chatty_user_real_get_name;
  klass->set_name = chatty_user_real_set_name;
  klass->get_avatar = chatty_user_real_get_avatar;
  klass->set_avatar_async  = chatty_user_real_set_avatar_async;
  klass->set_avatar_finish = chatty_user_real_set_avatar_finish;

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

  /**
   * ChattyUser:protocols:
   *
   * Protocols supported by the User.  A user may
   * support more than one protocol.
   */
  properties[PROP_PROTOCOLS] =
    g_param_spec_int ("protocols",
                      "Protocols",
                      "Protocols supported by user",
                      CHATTY_PROTOCOL_NONE,
                      CHATTY_PROTOCOL_TELEGRAM,
                      CHATTY_PROTOCOL_NONE,
                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

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
 * chatty_user_get_protocols:
 * @self: a #ChattyUser
 *
 * Get the protocols supported/implemented by @self.
 * There can be more than one protocol supported by
 * @self.
 *
 * Returns: %ChattyProtocol flag
 */
ChattyProtocol
chatty_user_get_protocols (ChattyUser *self)
{
  g_return_val_if_fail (CHATTY_IS_USER (self), CHATTY_PROTOCOL_NONE);

  return CHATTY_USER_GET_CLASS (self)->get_protocols (self);
}

gboolean
chatty_user_matches (ChattyUser     *self,
                     const char     *needle,
                     ChattyProtocol  protocols,
                     gboolean        match_name)
{
  g_return_val_if_fail (CHATTY_IS_USER (self), FALSE);

  if (!(protocols & chatty_user_get_protocols (self)))
    return FALSE;

  if (!needle || !*needle)
    return TRUE;

  if (match_name)
    {
      gboolean match;

      match = chatty_user_real_matches (self, needle, protocols, match_name);

      if (match)
        return match;
    }

  /* We have done this already, and if we reached here, we have no match */
  if (CHATTY_USER_GET_CLASS (self)->matches == chatty_user_real_matches)
    return FALSE;

  return CHATTY_USER_GET_CLASS (self)->matches (self, needle, protocols, match_name);
}

/**
 * chatty_user_compare:
 * @a: a #ChattyUser
 * @b: a #ChattyUser
 *
 * Compare to users and find the order they
 * should be sorted.
 *
 * Returns: < 0 if @a before @b, 0 if they
 * compare equal, > 0 if @a compares after @b.
 */
int
chatty_user_compare (ChattyUser *a,
                     ChattyUser *b)
{
  g_return_val_if_fail (CHATTY_IS_USER (a), 0);
  g_return_val_if_fail (CHATTY_IS_USER (b), 0);

  return g_utf8_collate (chatty_user_get_name (a),
                         chatty_user_get_name (b));
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

void
chatty_user_set_avatar_async (ChattyUser          *self,
                              const char          *file_name,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_return_if_fail (CHATTY_IS_USER (self));

  CHATTY_USER_GET_CLASS (self)->set_avatar_async (self, file_name, cancellable,
                                                  callback, user_data);
}

gboolean
chatty_user_set_avatar_finish (ChattyUser    *self,
                               GAsyncResult  *result,
                               GError       **error)
{
  g_return_val_if_fail (CHATTY_IS_USER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return CHATTY_USER_GET_CLASS (self)->set_avatar_finish (self, result, error);
}
