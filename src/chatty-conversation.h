/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __CONVERSATION_H_INCLUDE__
#define __CONVERSATION_H_INCLUDE__


typedef struct chatty_log                ChattyLog;
typedef struct chatty_conversation       ChattyConversation;
typedef struct chatty_conv_view_header   ChattyConvViewHeader;


#define CHATTY_CONVERSATION(conv) \
  ((ChattyConversation *)(conv)->ui_data)

#define CHATTY_IS_CHATTY_CONVERSATION(conv) \
    (purple_conversation_get_ui_ops (conv) == \
     chatty_conversations_get_conv_ui_ops())

struct chatty_conv_view_header {
  GtkWidget *icon;
  gchar     *name;
};

struct chatty_log {
  char *time_stamp;
  char *name;
  char *msg;
};

struct chatty_conversation {
  PurpleConversation  *active_conv;
  GList               *convs;
  ChattyMsgList       *msg_list;
  GtkWidget           *msg_entry;
  GtkTextBuffer       *msg_buffer;
  GtkWidget           *button_send;
  GtkWidget           *tab_cont;
  GtkWidget           *icon;
  guint                unseen_count;
  guint                unseen_state;

  struct {
    int timer;
    GList *current;
  } attach;

  ChattyConvViewHeader *conv_header;
};


typedef enum
{
  CHATTY_UNSEEN_NONE,
  CHATTY_UNSEEN_NO_LOG,
  CHATTY_UNSEEN_TEXT,
} ChattyUnseenState;


PurpleConversationUiOps *chatty_conversations_get_conv_ui_ops(void);

void chatty_conv_im_with_buddy (PurpleAccount *account, const char *username);
void *chatty_conversations_get_handle (void);
void chatty_conversations_init (void);
void chatty_conversations_uninit (void);
GList *chatty_conv_find_unseen (ChattyUnseenState  state);
void chatty_conv_set_unseen (ChattyConversation *chatty_conv,
                             ChattyUnseenState   state);


#endif
