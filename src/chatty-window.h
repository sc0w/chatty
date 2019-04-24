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
  GtkApplication    *app;
  GtkWindow         *main_window;
  HdyLeaflet        *content_box;
  HdyLeaflet        *header_box;
  HdyHeaderGroup    *header_group;
  GtkWidget         *dialog_settings;
  GtkWidget         *dialog_new_chat;
  GtkWidget         *dialog_muc_info;
  GtkStack          *stack_panes_main;
  HdySearchBar      *search_bar_chats;
  GtkEntry          *search_entry_chats;
  GtkEntry          *search_entry_contacts;
  PurpleAccount     *selected_account;
  GtkListBox        *list_manage_account;
  GtkBox            *pane_view_chat_list;
  GtkBox            *pane_view_muc_info;
  GtkWidget         *pane_view_message_list;
  GtkBox            *pane_view_new_chat;
  GtkHeaderBar      *sub_header_bar;
  GtkWidget         *sub_header_icon;
  GtkWidget         *sub_header_label;
  GtkWidget         *header_spinner;
  GSList            *radio_button_list;
  GtkWidget         *dummy_prefix_radio;
  GtkWidget         *label_contact_id;
  GtkWidget         *button_menu_chat_info;
  GtkWidget         *button_menu_add_contact;
  GtkWidget         *button_header_chat_info;
  GtkWidget         *separator_menu_msg_view;

  struct {
    GtkWidget     *label_chat_id;
    GtkWidget     *label_topic;
    GtkWidget     *label_num_user;
    GtkWidget     *button_edit_topic;
    GtkWidget     *box_topic_editor;
    GtkTextBuffer *msg_buffer_topic;
    GtkSwitch     *switch_prefs_notifications;
    GtkSwitch     *switch_prefs_persistant;
    GtkSwitch     *switch_prefs_autojoin;
  } muc;

} chatty_data_t;

chatty_data_t *chatty_get_data(void);

#define CHATTY_COLOR_GREEN  "6BBA3D"
#define CHATTY_COLOR_BLUE   "4A8FD9"
#define CHATTY_COLOR_PURPLE "842B84"
#define CHATTY_COLOR_GREY   "B2B2B2"

enum {
  CHATTY_PREF_SEND_RECEIPTS,
  CHATTY_PREF_MESSAGE_CARBONS,
  CHATTY_PREF_TYPING_NOTIFICATION,
  CHATTY_PREF_SHOW_OFFLINE,
  CHATTY_PREF_INDICATE_OFFLINE,
  CHATTY_PREF_INDICATE_IDLE,
  CHATTY_PREF_INDICATE_UNKNOWN,
  CHATTY_PREF_CONVERT_SMILEY,
  CHATTY_PREF_RETURN_SENDS,
  CHATTY_PREF_MUC_NOTIFICATIONS,
  CHATTY_PREF_MUC_AUTOJOIN,
  CHATTY_PREF_MUC_PERSISTANT,
  CHATTY_PREF_LAST
} ChattyPreferences;


typedef enum {
  CHATTY_VIEW_NEW_CHAT,
  CHATTY_VIEW_CHAT_LIST,
  CHATTY_VIEW_CHAT_INFO,
  CHATTY_VIEW_JOIN_CHAT,
  CHATTY_VIEW_MESSAGE_LIST,
  CHATTY_VIEW_SETTINGS,
  CHATTY_VIEW_ABOUT_CHATTY
} ChattyWindowState;


typedef enum {
  CHATTY_MESSAGE_MODE_XMPP,
  CHATTY_MESSAGE_MODE_SMS
} ChattyWindowMessageMode;

void chatty_window_change_view (guint state);
void chatty_window_activate (GtkApplication* app, gpointer user_data);
void chatty_window_update_sub_header_titlebar (GdkPixbuf  *icon, const char *title);

#endif
