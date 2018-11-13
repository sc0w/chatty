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
  GtkTreeView       *treeview_chats;
  GtkListStore      *treemodel_chats;
  GtkTreeView       *treeview_contacts;
  GtkListStore      *treemodel_contacts;
  GtkWidget         *search_entry;
  GtkTreeViewColumn *text_column;
  GList             *blist_nodes;
  PurpleBlistNode   *selected_node;
  guint             *messaging_mode;
  guint             refresh_timer;
  gint              filter_timeout;
  gpointer          priv;
} ChattyBuddyList;


typedef enum {
  CHATTY_BLIST_NODE_HAS_PENDING_MESSAGE            =  1 << 0,
  CHATTY_BLIST_CHAT_HAS_PENDING_MESSAGE_WITH_NICK  =  1 << 1,
} ChattyBlistNodeFlags;


typedef struct _chatty_blist_node {
  GtkTreeRowReference *row_chat;
  GtkTreeRowReference *row_contact;
  GtkTreeIter          iter;
  gboolean             contact_expanded;
  gboolean             recent_signonoff;
  gint                 recent_signonoff_timer;

  struct {
    PurpleConversation   *conv;
    guint                 pending_messages;
    GDateTime            *last_msg_timestamp;
    ChattyBlistNodeFlags  flags;
  } conv;
} ChattyBlistNode;


typedef struct {
  char  *buddy_name;
  char  *buddy_nick;
  char  *invite_msg;
} ChattyBlistAddBuddyData;


enum
{
  COLUMN_NODE,
  COLUMN_AVATAR,
  COLUMN_NAME,
  COLUMN_LAST,
  NUM_COLUMNS
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
void chatty_blist_returned_from_chat (void);
void chatty_blist_add_buddy (PurpleAccount *account);
void chatty_blist_refresh (PurpleBuddyList *list, gboolean remove);
void chatty_blist_create_add_buddy_view (PurpleAccount *account);
void chatty_blist_chat_list_remove_buddy (void);

#endif
