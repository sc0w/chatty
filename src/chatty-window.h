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
#define HANDY_USE_UNSTABLE_API
#include <handy.h>

typedef struct {
  GtkWindow         *main_window;
  HdyLeaflet        *content_box;
  HdyLeaflet        *header_box;
  HdyHeaderGroup    *header_group;
  GtkWidget         *dialog_settings;
  GtkWidget         *dialog_new_chat;
  GtkStack          *stack_panes_main;
  HdySearchBar      *search_bar_chats;
  GtkEntry          *search_entry_chats;
  GtkEntry          *search_entry_contacts;
  PurpleAccount     *selected_account;
  GtkListBox        *list_manage_account;
  GtkBox            *pane_view_chat_list;
  GtkWidget         *pane_view_message_list;
  GtkBox            *pane_view_new_chat;
  GtkHeaderBar      *sub_header_bar;
  GtkWidget         *sub_header_icon;
  GtkWidget         *sub_header_label;
  GtkWidget         *label_contact_id;
  GtkWidget         *header_spinner;
  GSList            *radio_button_list;
  GtkWidget         *dummy_prefix_radio;
  gint               view_state_last;
  gint               view_state_next;
} chatty_data_t;

chatty_data_t *chatty_get_data(void);

enum {
  CHATTY_PREF_SEND_RECEIPTS,
  CHATTY_PREF_MESSAGE_CARBONS,
  CHATTY_PREF_TYPING_NOTIFICATION,
  CHATTY_PREF_SHOW_OFFLINE,
  CHATTY_PREF_INDICATE_OFFLINE,
  CHATTY_PREF_INDICATE_IDLE,
  CHATTY_PREF_CONVERT_SMILEY,
  CHATTY_PREF_RETURN_SENDS,
  CHATTY_PREF_LAST
} ChattyPreferences;


typedef enum {
  CHATTY_VIEW_NEW_CHAT,
  CHATTY_VIEW_CHAT_LIST,
  CHATTY_VIEW_JOIN_CHAT,
  CHATTY_VIEW_MESSAGE_LIST,
  CHATTY_VIEW_SETTINGS
} ChattyWindowState;


typedef enum {
  CHATTY_MESSAGE_MODE_XMPP,
  CHATTY_MESSAGE_MODE_SMS
} ChattyWindowMessageMode;

void chatty_window_change_view (guint state);
void chatty_window_activate (GtkApplication* app, gpointer user_data);
void chatty_window_update_sub_header_titlebar (GdkPixbuf  *icon, const char *title);

#endif
