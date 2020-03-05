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

#define CHATTY_COLOR_GREEN     "6BBA3D"
#define CHATTY_COLOR_BLUE      "4A8FD9"
#define CHATTY_COLOR_PURPLE    "842B84"
#define CHATTY_COLOR_GREY      "B2B2B2"
#define CHATTY_COLOR_DIM_GREY  "797979"
#define CHATTY_COLOR_DARK_GREY "323232"

typedef enum {
  CHATTY_OVERLAY_EMPTY_CHAT,
  CHATTY_OVERLAY_EMPTY_CHAT_NO_SMS,
  CHATTY_OVERLAY_EMPTY_CHAT_NO_IM,
  CHATTY_OVERLAY_EMPTY_CHAT_NO_SMS_IM,
} ChattyOverlayMode;


typedef enum {
  CHATTY_ACCOUNTS_NONE   = 0,
  CHATTY_ACCOUNTS_SMS    = 1 << 0,
  CHATTY_ACCOUNTS_IM     = 1 << 1,
  CHATTY_ACCOUNTS_IM_SMS = 3
} ChattyActiveAccounts;


typedef enum {
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
                              const char     *uri);

void chatty_window_change_view (ChattyWindow *self, guint state);
void chatty_window_update_sub_header_titlebar (ChattyWindow *self, GdkPixbuf *icon, const char *title);

GtkWidget *chatty_window_get_chats_listbox (ChattyWindow *self);
GtkWidget *chatty_window_get_convs_notebook (ChattyWindow *self);

void chatty_window_set_overlay_visible (ChattyWindow *self, gboolean visible);
void chatty_window_set_new_chat_dialog_visible (ChattyWindow *self, gboolean visible);

void chatty_window_set_menu_add_contact_button_visible (ChattyWindow *self, gboolean visible);
void chatty_window_set_menu_add_in_contacts_button_visible (ChattyWindow *self, gboolean visible);
void chatty_window_set_header_chat_info_button_visible (ChattyWindow *self, gboolean visible);


G_END_DECLS
