/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __CONVERSATION_H_INCLUDE__
#define __CONVERSATION_H_INCLUDE__

#include <gtk/gtk.h>
#include <gtk/gtkwidget.h>
#include "purple.h"
#include "chatty-message-list.h"

typedef struct chatty_log                ChattyLog;
typedef struct chatty_conversation       ChattyConversation;


#define CHATTY_CONVERSATION(conv) \
  ((ChattyConversation *)(conv)->ui_data)

struct chatty_log {
  time_t   epoch;  // TODO: @LELAND: Once log-parsing functions are cleaned, review this
  char    *msg;
  int      dir;
};

struct chatty_conversation {
  PurpleConversation  *conv;

  GtkWidget     *chat_view;
  ChattyMsgList *msg_list;
  GtkWidget     *msg_bubble_footer;
  GtkWidget     *tab_cont;

  guint     unseen_count;
  guint     unseen_state;

  char     *oldest_message_displayed;

  struct {
    GtkImage   *symbol_encrypt;
    gboolean    enabled;
  } omemo;
};


typedef enum
{
  CHATTY_UNSEEN_NONE,
  CHATTY_UNSEEN_NO_LOG,
  CHATTY_UNSEEN_TEXT,
} ChattyUnseenState;


typedef enum
{
  CHATTY_SMS_RECEIPT_NONE      = -1,
  CHATTY_SMS_RECEIPT_MM_ACKN   =  0,
  CHATTY_SMS_RECEIPT_SMSC_ACKN,
} e_sms_receipt_states;


void chatty_conv_im_with_buddy (PurpleAccount *account, const char *username);
void chatty_conv_show_conversation (PurpleConversation *conv);
void chatty_conv_join_chat (PurpleChat *chat);
void *chatty_conversations_get_handle (void);
void chatty_conversations_init (void);
void chatty_conversations_uninit (void);
ChattyConversation * chatty_conv_container_get_active_chatty_conv (GtkNotebook *notebook);
void chatty_conv_set_unseen (ChattyConversation *chatty_conv,
                             ChattyUnseenState   state);
void chatty_conv_add_history_since_component(GHashTable *components, const char *account, const char *room);



#endif
