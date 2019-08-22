/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-notify"

#include <glib.h>
#include <glib/gi18n.h>
#include "purple.h"
#include <libnotify/notify.h>
#include "chatty-window.h"
#include "chatty-notify.h"


static void
cb_open_message (NotifyNotification *notification,
                 const char         *action,
                 PurpleBlistNode    *node)
{
  // TODO: switch to the conversation view

  notify_notification_close (notification, NULL);
}


static void
cb_open_settings (NotifyNotification *notification,
                  const char         *action,
                  gpointer            user_data)
{
  chatty_window_change_view (CHATTY_VIEW_SETTINGS);

  notify_notification_close (notification, NULL);
}


static void
cb_notification_closed (NotifyNotification *notification)
{
  g_object_unref (notification);
}


void
chatty_notify_show_notification (const char      *message,
                                 guint            notification_type,
                                 PurpleBlistNode *node)
{
  NotifyNotification *notification;

  if (!message) {
    return;
  }

  notification = notify_notification_new (_("Chatty"), message, "sm.puri.Chatty-symbolic");

  g_signal_connect (notification,
                    "closed",
                    G_CALLBACK (cb_notification_closed),
                    NULL);

  notify_notification_set_category (notification, "messaging");
  notify_notification_set_hint (notification, "transient", g_variant_new_boolean (TRUE));
  notify_notification_set_hint (notification, "desktop-entry", g_variant_new_string (_("Chatty")));

  switch (notification_type) {
    case CHATTY_NOTIFY_TYPE_MESSAGE:
      notify_notification_set_urgency (notification, NOTIFY_URGENCY_LOW);
      notify_notification_set_timeout (notification, NOTIFY_EXPIRES_DEFAULT);

      notify_notification_add_action (notification,
                                      "open-message",
                                      _("Open Message"),
                                      (NotifyActionCallback) cb_open_message,
                                      PURPLE_BLIST_NODE(node),
                                      g_free);
      break;

    case CHATTY_NOTIFY_TYPE_ACCOUNT:
      notify_notification_set_urgency (notification, NOTIFY_URGENCY_NORMAL);
      notify_notification_set_timeout (notification, NOTIFY_EXPIRES_DEFAULT);

      notify_notification_add_action (notification,
                                      "open-settings",
                                      _("Open Account Settings"),
                                      (NotifyActionCallback) cb_open_settings,
                                      NULL,
                                      g_free);
      break;

    case CHATTY_NOTIFY_TYPE_GENERIC:
      notify_notification_set_urgency (notification, NOTIFY_URGENCY_LOW);
      notify_notification_set_timeout (notification, NOTIFY_EXPIRES_DEFAULT);
      break;

    case CHATTY_NOTIFY_TYPE_ERROR:
      notify_notification_set_urgency (notification, NOTIFY_URGENCY_CRITICAL);
      notify_notification_set_timeout (notification, NOTIFY_EXPIRES_DEFAULT);
      break;

    default:
      notify_notification_set_urgency (notification, NOTIFY_URGENCY_LOW);
      notify_notification_set_timeout (notification, NOTIFY_EXPIRES_DEFAULT);
  }

  if (!notify_notification_show (notification, NULL)) {
    g_debug ("%s: Failed to show notification", __func__);
  }
}
