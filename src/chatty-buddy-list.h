/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __BUDDY_LIST_H_INCLUDE__
#define __BUDDY_LIST_H_INCLUDE__

#include "purple.h"
#include "users/chatty-pp-buddy.h"
#include "chatty-contact-row.h"


typedef struct {
  char  *buddy_name;
  char  *buddy_nick;
  char  *invite_msg;
} ChattyBlistAddBuddyData;


#define CHATTY_IS_CHATTY_BLIST(list) \
  (purple_blist_get_ui_ops() == chatty_blist_get_ui_ops())

PurpleBlistUiOps *chatty_blist_get_ui_ops (void);

void chatty_blist_init (void);
void chatty_blist_uninit (void);
void chatty_blist_chat_list_select_first (void);
void chatty_blist_returned_from_chat (void);
void chatty_blist_refresh (void);
void chatty_blist_create_add_buddy_view (PurpleAccount *account);
void chatty_blist_contact_list_add_buddy (void);
void chatty_blist_gnome_contacts_add_buddy (void);
void chatty_blist_chat_list_leave_chat (void);
void chatty_blist_chat_list_remove_buddy (void);
int chatty_blist_list_has_children (int list_type);
gboolean chatty_blist_protocol_is_sms (PurpleAccount *account);
void chatty_blist_add_buddy_from_uri (const char *uri);
void chatty_blist_enable_folks_contacts (void);
void chatty_blist_join_group_chat (PurpleAccount *account,
                                   const char    *group_chat_id,
                                   const char    *room_alias,
                                   const char    *user_alias,
                                   const char    *pwd);

#endif
