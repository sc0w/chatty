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

int chatty_history_open (void);

void chatty_history_close (void);

void chatty_history_add_chat_message (const char *stanza,
                                      int         direction,
                                      const char *account,
                                      const char *who,
                                      const char *uid,
                                      time_t      m_time,
                                      const char *room);

void chatty_history_add_im_message (const char *stanza,
                                    int         direction,
                                    const char *account,
                                    const char *who,
                                    const char *uid,
                                    time_t      m_time);


void chatty_history_get_im_messages (const char* account,
                                     const char* who,
                                     void (*cb)(const unsigned char* msg,
                                                int direction,
                                                time_t time_stamp,
                                                const unsigned char* uuid,
                                                ChattyConversation *chatty_conv,
                                                int last_message),
                                     ChattyConversation *chatty_conv,
                                     guint      limit,
                                     char       *oldest_message_displayed);

// TODO: @LELAND: chatty_conv should be a gpointer
void
chatty_history_get_chat_messages (const char *account,
                                  const char *room,
                                  void (*cb)(const unsigned char* msg,
                                            int direction,
                                            int time_stamp,
                                            const char* room,
                                            const unsigned char *who,
                                            const unsigned char* uuid,
                                            ChattyConversation *chatty_conv),
                                  ChattyConversation *chatty_conv,
                                  guint      limit,
                                  char       *oldest_message_displayed);

int
chatty_history_get_chat_last_message_time (const char* account,
                                           const char* room);


void
chatty_history_delete_chat (const char* account,
                            const char* room);

void
chatty_history_delete_im (const char *account,
                          const char *who);


#endif
