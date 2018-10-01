/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __BUDDY_LIST_H_INCLUDE__
#define __BUDDY_LIST_H_INCLUDE__

#include "purple.h"


typedef struct {
  GtkBox            *box;
  GtkTreeView       *treeview;
  GtkListStore      *treemodel;
  GtkTreeViewColumn *text_column;
  GtkScrolledWindow *scroll;
  GdkPixbuf         *empty_avatar;

  PurpleBlistNode   *selected_node;

  guint             *messaging_mode;
  guint             refresh_timer;

  gpointer          priv;
} ChattyBuddyList;


typedef enum {
  CHATTY_BLIST_NODE_HAS_PENDING_MESSAGE            =  1 << 0,
  CHATTY_BLIST_CHAT_HAS_PENDING_MESSAGE_WITH_NICK  =  1 << 1,
} ChattyBlistNodeFlags;


typedef struct _chatty_blist_node {
  GtkTreeRowReference *row;
  GtkTreeIter         iter;
  gboolean            contact_expanded;
  gboolean            recent_signonoff;
  gint                recent_signonoff_timer;

  struct {
    PurpleConversation   *conv;
    time_t               last_message;
    ChattyBlistNodeFlags flags;
  } conv;
} ChattyBlistNode;


enum
{
  COLUMN_NODE,
  COLUMN_AVATAR,
  COLUMN_NAME,
  COLUMN_TIME,
  NUM_COLUMNS,
};


enum {
  CHATTY_MSG_MODE_XMPP,
  CHATTY_MSG_MODE_OMEMO,
  CHATTY_MSG_MODE_SMS
} e_messaging_modes;


enum {
  CHATTY_STATUS_ICON_LARGE,
  CHATTY_STATUS_ICON_SMALL
} e_icon_size;


#define CHATTY_BLIST(list) ((ChattyBuddyList *)purple_blist_get_ui_data())
#define CHATTY_IS_CHATTY_BLIST(list) \
  (purple_blist_get_ui_ops() == chatty_blist_get_ui_ops())

PurpleBlistUiOps *chatty_blist_get_ui_ops (void);

void chatty_blist_init (void);
void chatty_blist_uninit (void);
void chatty_blist_add_buddy (void);

#endif
