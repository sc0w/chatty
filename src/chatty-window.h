/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#pragma once


#include <gtk/gtk.h>
#include "chatty-settings.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_WINDOW (chatty_window_get_type())

G_DECLARE_FINAL_TYPE (ChattyWindow, chatty_window, CHATTY, WINDOW, GtkApplicationWindow)


struct _ChattyWindow
{
  GtkApplicationWindow parent_instance;

  ChattySettings *settings;

  GtkWidget *listbox_chats;

  GtkWidget *content_box;
  GtkWidget *header_box;
  GtkWidget *header_group;

  GtkWidget *sub_header_icon;
  GtkWidget *sub_header_label;

  GtkWidget *dialog_new_chat;
  GtkWidget *dialog_muc_info;

  GtkWidget *search_bar_chats;
  GtkWidget *search_entry_chats;

  GtkWidget *button_menu_add_contact;
  GtkWidget *button_menu_add_gnome_contact;
  GtkWidget *button_menu_new_group_chat;
  GtkWidget *button_header_chat_info;
  GtkWidget *button_header_add_chat;
  GtkWidget *button_header_sub_menu;

  GtkWidget *pane_view_chat_list;

  GtkWidget *notebook_convs;

  GtkWidget *overlay;
  GtkWidget *icon_overlay;
  GtkWidget *label_overlay_1;
  GtkWidget *label_overlay_2;
  GtkWidget *label_overlay_3;

  const char *uri;
  
  gboolean daemon_mode;
  gboolean im_account_connected;
  gboolean sms_account_connected;
};


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


enum {
  LURCH_STATUS_DISABLED = 0,  // manually disabled
  LURCH_STATUS_NOT_SUPPORTED, // no OMEMO support, i.e. there is no devicelist node
  LURCH_STATUS_NO_SESSION,    // OMEMO is supported, but there is no libsignal session yet
  LURCH_STATUS_OK             // OMEMO is supported and session exists
} e_lurch_status;


typedef enum {
  CHATTY_VIEW_NEW_CHAT,
  CHATTY_VIEW_CHAT_LIST,
  CHATTY_VIEW_CHAT_INFO,
  CHATTY_VIEW_JOIN_CHAT,
  CHATTY_VIEW_MESSAGE_LIST,
  CHATTY_VIEW_SETTINGS,
  CHATTY_VIEW_ABOUT_CHATTY
} ChattyWindowState;


GtkWidget *chatty_window_new (GtkApplication *application, 
                              gboolean        daemon_mode, 
                              ChattySettings *settings, 
                              char           *uri);

void chatty_window_change_view (ChattyWindow *self, guint state);
void chatty_window_update_sub_header_titlebar (ChattyWindow *self, GdkPixbuf *icon, const char *title);

GtkWidget *chatty_window_get_search_entry (ChattyWindow *self);
GtkWidget *chatty_window_get_listbox_chats (ChattyWindow *self);
GtkWidget *chatty_window_get_notebook_convs (ChattyWindow *self);
GtkWidget *chatty_window_get_new_chat_dialog (ChattyWindow *self);
const char *chatty_window_get_uri (ChattyWindow *self);

void chatty_window_set_overlay_visible (ChattyWindow *self, gboolean visible);
void chatty_window_set_dialog_new_chat_visible (ChattyWindow *self, gboolean visible);

void chatty_window_set_button_menu_add_contact_visible (ChattyWindow *self, gboolean visible);
void chatty_window_set_button_menu_add_gnome_contact_visible (ChattyWindow *self, gboolean visible);
void chatty_window_set_button_header_chat_info_visible (ChattyWindow *self, gboolean visible);
void chatty_window_set_button_group_chat_sensitive (ChattyWindow *self, gboolean sensitive);
void chatty_window_set_button_header_add_chat_sensitive (ChattyWindow *self, gboolean sensitive);
void chatty_window_set_button_header_sub_menu_sensitive (ChattyWindow *self, gboolean sensitive);

void chatty_window_set_im_account_connected (ChattyWindow *self, gboolean connected);
gboolean chatty_window_get_im_account_connected (ChattyWindow *self);
void chatty_window_set_sms_account_connected (ChattyWindow *self, gboolean connected);
gboolean chatty_window_get_sms_account_connected (ChattyWindow *self);


G_END_DECLS
