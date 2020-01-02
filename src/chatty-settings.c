/* chatty-settings.c
 *
 * Copyright 2019 Purism SPC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-settings"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "chatty-settings.h"

/**
 * SECTION: chatty-settings
 * @title: ChattySettings
 * @short_description: The Application settings
 * @include: "chatty-settings.h"
 *
 * A class that handles application specific settings, and
 * to store them to disk.
 */

struct _ChattySettings
{
  GObject     parent_instance;

  GSettings  *settings;
};

G_DEFINE_TYPE (ChattySettings, chatty_settings, G_TYPE_OBJECT)


enum {
  PROP_0,
  PROP_FIRST_START,
  PROP_SEND_RECEIPTS,
  PROP_SEND_TYPING,
  PROP_GREYOUT_OFFLINE_BUDDIES,
  PROP_BLUR_IDLE_BUDDIES,
  PROP_INDICATE_UNKNOWN_CONTACTS,
  PROP_CONVERT_EMOTICONS,
  PROP_RETURN_SENDS_MESSAGE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
chatty_settings_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  ChattySettings *self = (ChattySettings *)object;

  switch (prop_id)
    {
    case PROP_FIRST_START:
      g_value_set_boolean (value, chatty_settings_get_first_start (self));
      break;

    case PROP_SEND_RECEIPTS:
      g_value_set_boolean (value, chatty_settings_get_send_receipts (self));
      break;

    case PROP_SEND_TYPING:
      g_value_set_boolean (value, chatty_settings_get_send_typing (self));
      break;

    case PROP_GREYOUT_OFFLINE_BUDDIES:
      g_value_set_boolean (value, chatty_settings_get_greyout_offline_buddies (self));
      break;

    case PROP_BLUR_IDLE_BUDDIES:
      g_value_set_boolean (value, chatty_settings_get_blur_idle_buddies (self));
      break;

    case PROP_INDICATE_UNKNOWN_CONTACTS:
      g_value_set_boolean (value, chatty_settings_get_indicate_unkown_contacts (self));
      break;

    case PROP_CONVERT_EMOTICONS:
      g_value_set_boolean (value, chatty_settings_get_convert_emoticons (self));
      break;

    case PROP_RETURN_SENDS_MESSAGE:
      g_value_set_boolean (value, chatty_settings_get_return_sends_message (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
chatty_settings_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  ChattySettings *self = (ChattySettings *)object;


  switch (prop_id)
    {
    case PROP_FIRST_START:
      g_settings_set_boolean (self->settings, "first-start",
                              g_value_get_boolean (value));
      break;

    case PROP_SEND_RECEIPTS:
      g_settings_set_boolean (self->settings, "send-receipts",
                              g_value_get_boolean (value));
      break;

    case PROP_SEND_TYPING:
      g_settings_set_boolean (self->settings, "send-typing",
                              g_value_get_boolean (value));
      break;

    case PROP_GREYOUT_OFFLINE_BUDDIES:
      g_settings_set_boolean (self->settings, "greyout-offline-buddies",
                              g_value_get_boolean (value));
      break;

    case PROP_BLUR_IDLE_BUDDIES:
      g_settings_set_boolean (self->settings, "blur-idle-buddies",
                              g_value_get_boolean (value));
      break;

    case PROP_INDICATE_UNKNOWN_CONTACTS:
      g_settings_set_boolean (self->settings, "indicate-unknown-contacts",
                              g_value_get_boolean (value));
      break;

    case PROP_CONVERT_EMOTICONS:
      g_settings_set_boolean (self->settings, "convert-emoticons",
                              g_value_get_boolean (value));
      break;

    case PROP_RETURN_SENDS_MESSAGE:
      g_settings_set_boolean (self->settings, "return-sends-message",
                              g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
chatty_settings_constructed (GObject *object)
{
  ChattySettings *self = (ChattySettings *)object;

  G_OBJECT_CLASS (chatty_settings_parent_class)->constructed (object);

  g_settings_bind (self->settings, "send-receipts",
                   self, "send-receipts", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "send-typing",
                   self, "send-typing", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "greyout-offline-buddies",
                   self, "greyout-offline-buddies", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "blur-idle-buddies",
                   self, "blur-idle-buddies", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "indicate-unknown-contacts",
                   self, "indicate-unknown-contacts", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "return-sends-message",
                   self, "return-sends-message", G_SETTINGS_BIND_DEFAULT);
}

static void
chatty_settings_finalize (GObject *object)
{
  ChattySettings *self = (ChattySettings *)object;

  g_settings_set_boolean (self->settings, "first-start", FALSE);
  g_object_unref (self->settings);

  G_OBJECT_CLASS (chatty_settings_parent_class)->finalize (object);
}

static void
chatty_settings_class_init (ChattySettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = chatty_settings_get_property;
  object_class->set_property = chatty_settings_set_property;
  object_class->constructed = chatty_settings_constructed;
  object_class->finalize = chatty_settings_finalize;

  properties[PROP_FIRST_START] =
    g_param_spec_boolean ("first-start",
                          "First Start",
                          "Whether the application is launched the first time",
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_SEND_RECEIPTS] =
    g_param_spec_boolean ("send-receipts",
                          "Send Receipts",
                          "Send message read receipts",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SEND_TYPING] =
      g_param_spec_boolean ("send-typing",
                            "Send Typing",
                            "Send typing notifications",
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_GREYOUT_OFFLINE_BUDDIES] =
      g_param_spec_boolean ("greyout-offline-buddies",
                            "Greyout Offline Buddies",
                            "Greyout Offline Buddies",
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_BLUR_IDLE_BUDDIES] =
      g_param_spec_boolean ("blur-idle-buddies",
                            "Blur Idle Buddies",
                            "Blur Idle Buddies",
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_INDICATE_UNKNOWN_CONTACTS] =
      g_param_spec_boolean ("indicate-unknown-contacts",
                            "Indicate Unknown Contacts",
                            "Indicate Unknown Contacts",
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_CONVERT_EMOTICONS] =
      g_param_spec_boolean ("convert-emoticons",
                            "Convert Emoticons",
                            "Convert Emoticons",
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_RETURN_SENDS_MESSAGE] =
      g_param_spec_boolean ("return-sends-message",
                            "Return Sends Message",
                            "Whether Return key sends message",
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
chatty_settings_init (ChattySettings *self)
{
  self->settings = g_settings_new ("sm.puri.Chatty");
}

/**
 * chatty_settings_get_default:
 *
 * Get the default settings
 *
 * Returns: (transfer none): A #ChattySettings.
 */
ChattySettings *
chatty_settings_get_default (void)
{
  static ChattySettings *self;

  if (!self)
    {
      self = g_object_new (CHATTY_TYPE_SETTINGS, NULL);
      g_object_add_weak_pointer (G_OBJECT (self), (gpointer *)&self);
    }

  return self;
}


/**
 * chatty_settings_bind_widget:
 * @self:    A #ChattySettings
 * key:      A key to bind
 * widget:   A GtkWidget to tie to
 * property: The widget property to bind
 *
 */
void
chatty_settings_bind_widget (ChattySettings *self,
                             const char     *key,
                             GtkWidget      *widget,
                             const char     *property)
{
  g_return_if_fail (CHATTY_IS_SETTINGS(self) || widget != NULL);

  g_settings_bind (self->settings, key,
                   widget, property,
                   G_SETTINGS_BIND_DEFAULT);
}


/**
 * chatty_settings_get_first_start:
 * @self: A #ChattySettings
 *
 * Get if the application is launching for the first time.
 *
 * Returns: %TRUE if the application is launching for the
 * first time. %FALSE otherwise.
 */
gboolean
chatty_settings_get_first_start (ChattySettings *self)
{
  g_return_val_if_fail (CHATTY_IS_SETTINGS (self), FALSE);

  return g_settings_get_boolean (self->settings, "first-start");
}

/**
 * chatty_settings_get_send_receipts:
 * @self: A #ChattySettings
 *
 * Get if the application should send others the information
 * whether a received message is read or not.
 *
 * Returns: %TRUE if the send receipts should be sent.
 * %FALSE otherwise.
 */
gboolean
chatty_settings_get_send_receipts (ChattySettings *self)
{
  g_return_val_if_fail (CHATTY_IS_SETTINGS (self), FALSE);

  return g_settings_get_boolean (self->settings, "send-receipts");
}

/**
 * chatty_settings_get_send_typing:
 * @self: A #ChattySettings
 *
 * Get if the typing notification should be sent to
 * other user.
 *
 * Returns: %TRUE if typing notification should be sent.
 * %FALSE otherwise
 */
gboolean
chatty_settings_get_send_typing (ChattySettings *self)
{
  g_return_val_if_fail (CHATTY_IS_SETTINGS (self), FALSE);

  return g_settings_get_boolean (self->settings, "send-typing");
}

gboolean
chatty_settings_get_greyout_offline_buddies (ChattySettings *self)
{
  g_return_val_if_fail (CHATTY_IS_SETTINGS (self), FALSE);

  return g_settings_get_boolean (self->settings, "greyout-offline-buddies");
}

gboolean
chatty_settings_get_blur_idle_buddies (ChattySettings *self)
{
  g_return_val_if_fail (CHATTY_IS_SETTINGS (self), FALSE);

  return g_settings_get_boolean (self->settings, "blur-idle-buddies");
}

gboolean
chatty_settings_get_indicate_unkown_contacts (ChattySettings *self)
{
  g_return_val_if_fail (CHATTY_IS_SETTINGS (self), FALSE);

  return g_settings_get_boolean (self->settings, "indicate-unknown-contacts");
}

gboolean
chatty_settings_get_convert_emoticons (ChattySettings *self)
{
  g_return_val_if_fail (CHATTY_IS_SETTINGS (self), FALSE);

  return g_settings_get_boolean (self->settings, "convert-emoticons");
}

gboolean
chatty_settings_get_return_sends_message (ChattySettings *self)
{
  g_return_val_if_fail (CHATTY_IS_SETTINGS (self), FALSE);

  return g_settings_get_boolean (self->settings, "return-sends-message");
}
