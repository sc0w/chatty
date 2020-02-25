/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-buddy.h
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>
#include <purple.h>

#include "chatty-contact-row.h"
#include "chatty-contact.h"
#include "chatty-item.h"
#include "chatty-enums.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_PP_BUDDY (chatty_pp_buddy_get_type ())

G_DECLARE_FINAL_TYPE (ChattyPpBuddy, chatty_pp_buddy, CHATTY, PP_BUDDY, ChattyItem)

/* Moved from chatty-buddy-list.h to allow compilation as static library. */
typedef struct _chatty_blist_node {
  ChattyContactRow *row_chat;
  ChattyContactRow *row_contact;
  ChattyPpBuddy    *buddy_object; /* Set only if node is buddy */
  gint              recent_signonoff_timer;

  struct {
    PurpleConversation   *conv;
    guint                 pending_messages;
    char                 *last_msg_timestamp;
    time_t                last_msg_ts_raw;
    char                  *last_message;
    int                   last_message_dir;
  } conv;
} ChattyBlistNode;

ChattyPpBuddy   *chatty_pp_buddy_get_object    (PurpleBuddy   *buddy);
PurpleAccount   *chatty_pp_buddy_get_account   (ChattyPpBuddy *self);
PurpleBuddy     *chatty_pp_buddy_get_buddy      (ChattyPpBuddy *self);
ChattyContact   *chatty_pp_buddy_get_contact   (ChattyPpBuddy *self);
void             chatty_pp_buddy_set_contact   (ChattyPpBuddy *self,
                                                ChattyContact *contact);

G_END_DECLS
