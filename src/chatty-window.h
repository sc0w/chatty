/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __WINDOW_H_INCLUDE__
#define __WINDOW_H_INCLUDE___

#include <gtk/gtk.h>
#include <purple.h>
#include "chatty-message-list.h"

typedef struct {
  GtkWindow         *main_window;
  GtkStack          *panes_stack;
  GtkStack          *conv_notebook;
  GtkBox            *blist_box;
  GtkTextBuffer     *msg_buffer;
  GtkWidget         *header_icon;
  GtkWidget         *header_title;
  GtkWidget         *header_button_left;
  GtkWidget         *header_button_right;
  GtkWidget         *button_send;
  GtkWidget         *button_add_buddy;
  GtkWidget         *button_connect;
  GtkEntry          *entry_account_name;
  GtkEntry          *entry_account_pwd;
  GtkEntry          *entry_buddy_name;
  GtkEntry          *entry_buddy_nick;
  GtkEntry          *entry_invite_msg;
  GtkLabel          *label_status;
  gboolean          sms_mode;
  gint              purple_state;
  gint              message_mode;
  gint              view_state;
  gint              view_state_next;
  gint              window_size_x;
  gint              window_size_y;

  PurpleAccount *account; // TODO to be removed when account-managment is implemented
} chatty_data_t;

chatty_data_t *chatty_get_data(void);

enum {
  CHATTY_PURPLE_CONNECTED,
  CHATTY_PURPLE_DISCONNECTED,
  CHATTY_PURPLE_CONNECTION_PENDING,
  /*  */
  CHATTY_OFONO_MODEM_ADDED,
  CHATTY_OFONO_SMS_SENT,
  CHATTY_OFONO_SMS_RECEIVED
} e_chatty_events;

enum {
  CHATTY_VIEW_LOGIN,
  CHATTY_VIEW_NEW_CHAT,
  CHATTY_VIEW_MESSAGE_LIST,
  CHATTY_VIEW_CHAT_LIST_SLIDE_RIGHT,
  CHATTY_VIEW_CHAT_LIST_SLIDE_DOWN
} e_window_state;

enum {
  CHATTY_MESSAGE_MODE_XMPP,
  CHATTY_MESSAGE_MODE_SMS
} e_message_mode;

void chatty_window_activate (GtkApplication* app, gpointer user_data);
void chatty_window_change_view (guint state);
void chatty_window_set_header_title (const char *title);

#endif
