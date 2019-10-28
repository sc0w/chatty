/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __BUDDY_LIST_H_INCLUDE__
#define __BUDDY_LIST_H_INCLUDE__

#include "purple.h"
#include "chatty-contact-row.h"

typedef enum {
  CHATTY_BLIST_NODE_HAS_PENDING_MESSAGE            =  1 << 0,
  CHATTY_BLIST_CHAT_HAS_PENDING_MESSAGE_WITH_NICK  =  1 << 1,
} ChattyBlistNodeFlags;


typedef struct _chatty_blist_node {
  ChattyContactRow *row_chat;
  ChattyContactRow *row_contact;
  gboolean          contact_expanded;
  gboolean          recent_signonoff;
  gint              recent_signonoff_timer;

  struct {
    PurpleConversation   *conv;
    guint                 pending_messages;
    char                 *last_msg_timestamp;
    time_t                last_msg_ts_raw;
    const char           *last_message;
    int                   last_message_dir;
    ChattyBlistNodeFlags  flags;
  } conv;
} ChattyBlistNode;


typedef struct {
  char  *buddy_name;
  char  *buddy_nick;
  char  *invite_msg;
} ChattyBlistAddBuddyData;


enum {
  CHATTY_MSG_MODE_XMPP,
  CHATTY_MSG_MODE_OMEMO,
  CHATTY_MSG_MODE_SMS
} e_messaging_modes;


enum
{
  COLUMN_NODE,
  COLUMN_AVATAR,
  COLUMN_NAME,
  COLUMN_LAST,
  NUM_COLUMNS
};


enum {
  CHATTY_STATUS_ICON_LARGE,
  CHATTY_STATUS_ICON_SMALL
} e_icon_size;


enum {
  CHATTY_CHAT_LIST_SELECT_ENABLED,
  CHATTY_CHAT_LIST_SELECT_DISABLED,
} e_list_mode;


#define CHATTY_IS_CHATTY_BLIST(list) \
  (purple_blist_get_ui_ops() == chatty_blist_get_ui_ops())

PurpleBlistUiOps *chatty_blist_get_ui_ops (void);

void chatty_blist_init (void);
void chatty_blist_uninit (void);
void chatty_blist_chat_list_select_first (void);
void chatty_blist_returned_from_chat (void);
void chatty_blist_add_buddy (const char *who, const char *whoalias);
void chatty_blist_refresh (PurpleBuddyList *list);
void chatty_blist_chat_list_selection (gboolean select);
void chatty_blist_create_add_buddy_view (PurpleAccount *account);
void chatty_blist_contact_list_add_buddy (void);
void chatty_blist_chat_list_leave_chat (void);
void chatty_blist_chat_list_remove_buddy (void);
int chatty_blist_list_has_children (int list_type);
gboolean chatty_blist_protocol_is_sms (PurpleAccount *account);
void chatty_blist_add_buddy_from_uri (const char *uri);
void chatty_blist_join_group_chat (PurpleAccount *account,
                                   const char    *group_chat_id,
                                   const char    *alias,
                                   const char    *pwd,
                                   gboolean       autojoin);

#endif
