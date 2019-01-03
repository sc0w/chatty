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
  GtkStack          *panes_stack;
  GtkBox            *pane_view_chat_list;
  GtkBox            *pane_view_new_contact;
  GtkBox            *pane_view_select_account;
  GtkWidget         *pane_view_message_list;
  GtkBox            *pane_view_new_chat;
  GtkBox            *pane_view_new_account;
  GtkHeaderBar      *header_view_message_list;
  GtkWidget         *header_icon;
  GtkWidget         *header_spinner;
  GtkWidget         *search_bar;
  GtkListBox        *account_list_manage;
  GtkListBox        *account_list_select;
  GtkListBox        *list_privacy_prefs;
  GtkListBox        *list_xmpp_prefs;
  GtkListBox        *list_editor_prefs;
  GtkSwitch         *prefs_switch_send_receipts;
  GtkSwitch         *prefs_switch_message_carbons;
  GtkSwitch         *prefs_switch_typing_notification;
  GtkSwitch         *prefs_switch_show_offline;
  GtkSwitch         *prefs_switch_indicate_offline;
  GtkSwitch         *prefs_switch_indicate_idle;
  GtkSwitch         *prefs_switch_convert_smileys;
  GtkSwitch         *prefs_switch_return_sends;
  HdyActionRow      *row_pref_message_carbons;
  GtkWidget         *button_add_contact;
  PurpleAccount     *contact_selected_account;
  GtkEntry          *entry_contact_name;
  GtkEntry          *entry_contact_nick;
  GtkEntry          *entry_invite_msg;
  GSList            *radio_button_list;
  GtkWidget         *dummy_prefix_radio;
  GtkWidget         *label_contact_id;
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
  CHATTY_VIEW_NEW_CONTACT,
  CHATTY_VIEW_NEW_ACCOUNT,
  CHATTY_VIEW_CHAT_LIST,
  CHATTY_VIEW_MESSAGE_LIST,
  CHATTY_VIEW_SETTINGS
} ChattyWindowState;


typedef enum {
  CHATTY_MESSAGE_MODE_XMPP,
  CHATTY_MESSAGE_MODE_SMS
} ChattyWindowMessageMode;

void chatty_window_activate (GtkApplication* app, gpointer user_data);
void chatty_window_change_view (guint state);
void chatty_window_set_header_title (const char *title);

#endif
