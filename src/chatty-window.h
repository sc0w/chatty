/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __WINDOW_H_INCLUDE__
#define __WINDOW_H_INCLUDE__

#include <gtk/gtk.h>
#include <purple.h>
#include "chatty-message-list.h"

typedef struct {
  GtkWindow         *main_window;
  GtkStack          *panes_stack;
  GtkBin            *pane_view_buddy_list;
  GtkWidget         *pane_view_message_list;
  GtkBin            *pane_view_manage_account;
  GtkBin            *pane_view_select_account;
  GtkBin            *pane_view_new_conversation;
  GtkBin            *pane_view_new_account;
  GtkWidget         *header_icon;
  GtkWidget         *header_title;
  GtkWidget         *header_button_left;
  GtkWidget         *header_button_right;
  GtkWidget         *button_add_buddy;
  GtkEntry          *entry_buddy_name;
  GtkEntry          *entry_buddy_nick;
  GtkEntry          *entry_invite_msg;
  gint              view_state_last;
  gint              view_state_next;
  gint              window_size_x;
  gint              window_size_y;
} chatty_data_t;

chatty_data_t *chatty_get_data(void);

enum {
  CHATTY_VIEW_NEW_CONVERSATION,
  CHATTY_VIEW_NEW_ACCOUNT,
  CHATTY_VIEW_MESSAGE_LIST,
  CHATTY_VIEW_MANAGE_ACCOUNT_LIST,
  CHATTY_VIEW_SELECT_ACCOUNT_LIST,
  CHATTY_VIEW_CONVERSATIONS_LIST,
} e_window_state;

enum {
  CHATTY_MESSAGE_MODE_XMPP,
  CHATTY_MESSAGE_MODE_SMS
} e_message_mode;

void chatty_window_activate (GtkApplication* app, gpointer user_data);
void chatty_window_change_view (guint state);
void chatty_window_set_header_title (const char *title);

#endif
