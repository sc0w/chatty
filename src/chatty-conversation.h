/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __CONVERSATION_H_INCLUDE__
#define __CONVERSATION_H_INCLUDE__

#include "chatty-message-list.h"

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
  PurpleConversation  *conv;

  ChattyMsgList *msg_list;
  GtkWidget     *msg_bubble_footer;
  GtkWidget     *tab_cont;
  GtkWidget     *icon;

  guint     unseen_count;
  guint     unseen_state;
  gboolean  notifications;

  struct {
    GtkWidget     *entry;
    GtkTextBuffer *buffer;
    GtkWidget     *scrolled;
    GtkWidget     *frame;
    GtkWidget     *button_send;
  } input;

  struct {
    GtkListBox *listbox_fp_own;
    GtkListBox *listbox_fp_contact;
    GtkImage   *symbol_encrypt;
    GtkSwitch  *switch_on_off;
    GtkWidget  *label_status_msg;
    const char *fp_own_device;
    guint       status;
    gboolean    enabled;
  } omemo;

  struct {
    GtkWidget   *list;
    GtkTreeView *treeview;
    guint        user_count;
    gboolean     notifications;
  } muc;

  struct {
    int    timer;
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


enum
{
  CHATTY_SMS_RECEIPT_NONE      = -1,
  CHATTY_SMS_RECEIPT_MM_ACKN   =  0,
  CHATTY_SMS_RECEIPT_SMSC_ACKN,
} e_sms_receipt_states;


enum
{
  MUC_COLUMN_AVATAR,
  MUC_COLUMN_ENTRY,
  MUC_COLUMN_NAME,
  MUC_COLUMN_ALIAS_KEY,
  MUC_COLUMN_LAST,
  MUC_COLUMN_FLAGS,
  MUC_NUM_COLUMNS
};


PurpleConversationUiOps *chatty_conversations_get_conv_ui_ops(void);

void chatty_conv_im_with_buddy (PurpleAccount *account, const char *username);
void chatty_conv_join_chat (PurpleChat *chat);
void chatty_conv_set_muc_topic (const char *topic_text);
void chatty_conv_set_muc_prefs (gint pref, gboolean value);
void chatty_conv_invite_muc_user (const char *user_name, const char *invite_msg);
void *chatty_conversations_get_handle (void);
void chatty_conv_container_init (void);
void chatty_conversations_init (void);
void chatty_conversations_uninit (void);
PurpleConversation * chatty_conv_container_get_active_purple_conv (GtkNotebook *notebook);
ChattyConversation * chatty_conv_container_get_active_chatty_conv (GtkNotebook *notebook);
ChattyLog* chatty_conv_message_get_last_msg (PurpleBuddy *buddy);
gboolean chatty_conv_delete_message_history (PurpleBuddy *buddy);
GList *chatty_conv_find_unseen (ChattyUnseenState  state);
void chatty_conv_set_unseen (ChattyConversation *chatty_conv,
                             ChattyUnseenState   state);


#endif
