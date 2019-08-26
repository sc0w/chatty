/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __NOTIFY_H_INCLUDE__
#define __NOTIFY_H_INCLUDE__


enum {
  CHATTY_NOTIFY_TYPE_MESSAGE,
  CHATTY_NOTIFY_TYPE_ACCOUNT,
  CHATTY_NOTIFY_TYPE_PLUGIN,
  CHATTY_NOTIFY_TYPE_GENERIC,
  CHATTY_NOTIFY_TYPE_ERROR
} ChattyNotifyType;


void chatty_notify_show_notification (const char      *message,
                                      guint            notification_type,
                                      const char     *buddy_name);

#endif
