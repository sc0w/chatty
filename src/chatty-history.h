/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __HISTORY_H_INCLUDE__
#define __HISTORY_H_INCLUDE__

#include "time.h"
#include "chatty-conversation.h"

//TODO:LELAND: Document methods!

int chatty_history_open(void);

void chatty_history_close(void);

void chatty_history_add_chat_message(int         conv_id,
                                     const char *stanza,
                                     int         direction,
                                     const char *jid,
                                     const char *alias,
                                     const char *uid,
                                     time_t      m_time,
                                     const char *conv_name);

void chatty_history_add_im_message(
                                   const char *stanza,
                                   int         direction,
                                   const char *account,
                                   const char *jid,
                                   const char *uid,
                                   time_t      m_time
                                   );

void chatty_history_remove_message(void);

void chatty_history_get_im_messages(const char* account,
                                    const char* jid,
                                    void (*callback)(char* msg, int direction, int time_stamp, ChattyConversation *chatty_conv),
                                    ChattyConversation *chatty_conv);

void
chatty_history_get_chat_messages(const char* conv_name,
                               void (*callback)(char* msg, int direction, int time_stamp, char* from, char *alias, ChattyConversation *chatty_conv),
                               ChattyConversation *chatty_conv);

int
chatty_history_get_chat_last_message_time(const char* conv_name);


void
chatty_history_delete_chat(const char* conv_name);

void
chatty_history_delete_im(const char *account, const char *jid);


#endif
