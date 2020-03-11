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
void chatty_blist_refresh (void);
gboolean chatty_blist_protocol_is_sms (PurpleAccount *account);

#endif
