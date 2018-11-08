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
  GtkBox            *pane_view_chat_list;
  GtkBox            *pane_view_new_contact;
  GtkBox            *pane_view_select_account;
  GtkWidget         *pane_view_message_list;
  GtkBox            *pane_view_manage_account;
  GtkBox            *pane_view_new_chat;
  GtkBox            *pane_view_new_account;
  GtkHeaderBar      *header_view_message_list;
  GtkWidget         *header_icon;
  GtkWidget         *button_add_buddy;
  GtkEntry          *entry_buddy_name;
  GtkEntry          *entry_buddy_nick;
  GtkEntry          *entry_invite_msg;
  GtkWidget         *label_buddy_id;
  gint               view_state_last;
  gint               view_state_next;
} chatty_data_t;

chatty_data_t *chatty_get_data(void);

typedef enum {
  CHATTY_VIEW_NEW_CHAT,
  CHATTY_VIEW_SELECT_ACCOUNT,
  CHATTY_VIEW_NEW_ACCOUNT,
  CHATTY_VIEW_MESSAGE_LIST,
  CHATTY_VIEW_MANAGE_ACCOUNT,
  CHATTY_VIEW_CHAT_LIST,
  CHATTY_VIEW_ADD_CONTACT
} ChattyWindowState;

typedef enum {
  CHATTY_MESSAGE_MODE_XMPP,
  CHATTY_MESSAGE_MODE_SMS
} ChattyWindowMessageMode;

void chatty_window_activate (GtkApplication* app, gpointer user_data);
void chatty_window_change_view (guint state);
void chatty_window_set_header_title (const char *title);

#endif
