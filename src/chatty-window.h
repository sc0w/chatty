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
#include "chatty-settings.h"
#define HANDY_USE_UNSTABLE_API
#include <handy.h>


typedef enum {
  CHATTY_CML_OPT_NONE     = 0,
  CHATTY_CML_OPT_DISABLE  = 1 << 0,
  CHATTY_CML_OPT_DEBUG    = 1 << 1,
  CHATTY_CML_OPT_VERBOSE  = 1 << 2
} ChattyCmlOptions;

typedef struct {
  /* Listboxes for sidebar and contact list */
  GtkListBox        *listbox_chats;
  GtkListBox        *listbox_contacts;

  HdyLeaflet        *content_box;
  HdyLeaflet        *header_box;
  HdyHeaderGroup    *header_group;

  GtkHeaderBar      *sub_header_bar;
  GtkWidget         *sub_header_icon;
  GtkWidget         *sub_header_label;

  GtkWidget         *dialog_settings;
  GtkWidget         *dialog_new_chat;
  GtkWidget         *dialog_muc_info;

  GtkStack          *stack_panes_main;
  GtkBox            *pane_view_chat_list;
  GtkBox            *pane_view_muc_info;
  GtkWidget         *pane_view_message_list;
  GtkBox            *pane_view_new_chat;

  HdySearchBar      *search_bar_chats;
  GtkEntry          *search_entry_chats;
  GtkEntry          *search_entry_contacts;

  GtkWidget         *button_menu_add_contact;
  GtkWidget         *button_menu_add_gnome_contact;
  GtkWidget         *button_menu_new_group_chat;
  GtkWidget         *button_header_chat_info;
  GtkWidget         *button_header_add_chat;
  GtkWidget         *button_header_sub_menu;

  PurpleAccount     *selected_account;
  GtkListBox        *list_manage_account;

  GSList            *radio_button_list;
  GtkWidget         *dummy_prefix_radio;

  GtkBox            *box_overlay;
  GtkImage          *icon_overlay;
  GtkWidget         *label_overlay_1;
  GtkWidget         *label_overlay_2;
  GtkWidget         *label_overlay_3;

  gboolean           im_account_connected;
  gboolean           sms_account_connected;

  GtkWidget         *settings_dialog;

  GListStore        *account_list;
  ChattyCmlOptions   cml_options;

  char        *uri;

  struct {
    GtkWidget     *label_chat_id;
    GtkWidget     *label_topic;
    GtkWidget     *label_title;
    GtkWidget     *label_num_user;
    GtkWidget     *button_edit_topic;
    GtkWidget     *box_topic_editor;
    GtkTextBuffer *msg_buffer_topic;
    GtkSwitch     *switch_prefs_notifications;
    GtkSwitch     *switch_prefs_status_msg;
    GtkSwitch     *switch_prefs_persistant;
    GtkSwitch     *switch_prefs_autojoin;
  } muc;
} chatty_data_t;

chatty_data_t *chatty_get_data(void);


typedef struct {
  const char  *title;
  const char  *text_1;
  const char  *text_2;
  const char  *icon_name;
  int          icon_size;
} overlay_content_t;


#define CHATTY_COLOR_GREEN     "6BBA3D"
#define CHATTY_COLOR_BLUE      "4A8FD9"
#define CHATTY_COLOR_PURPLE    "842B84"
#define CHATTY_COLOR_GREY      "B2B2B2"
#define CHATTY_COLOR_DIM_GREY  "797979"
#define CHATTY_COLOR_DARK_GREY "323232"

enum {
  CHATTY_PREF_MUC_NOTIFICATIONS,
  CHATTY_PREF_MUC_STATUS_MSG,
  CHATTY_PREF_MUC_AUTOJOIN,
  CHATTY_PREF_MUC_PERSISTANT,
  CHATTY_PREF_LAST
} ChattyPreferences;


enum {
  CHATTY_OVERLAY_EMPTY_CHAT,
  CHATTY_OVERLAY_EMPTY_CHAT_NO_SMS,
  CHATTY_OVERLAY_EMPTY_CHAT_NO_IM,
  CHATTY_OVERLAY_EMPTY_CHAT_NO_SMS_IM,
} ChattyOverlayMode;


enum {
  CHATTY_LIST_CHATS,
  CHATTY_LIST_CONTACTS,
  CHATTY_LIST_MUC
} ChattyListType;


enum {
  CHATTY_ACCOUNTS_NONE   = 0,
  CHATTY_ACCOUNTS_SMS    = 1 << 0,
  CHATTY_ACCOUNTS_IM     = 1 << 1,
  CHATTY_ACCOUNTS_IM_SMS = 3
} ChattyActiveAccounts;


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
void chatty_window_overlay_show (gboolean show);
void chatty_window_overlay_show_delayed (void);
void chatty_window_activate (GtkApplication* app, gpointer user_data);
void chatty_window_update_sub_header_titlebar (GdkPixbuf  *icon, const char *title);

#endif
