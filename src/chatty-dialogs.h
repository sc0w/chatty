/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __DIALOGS_H_INCLUDE__
#define __DIALOGS_H_INCLUDE__

#include <gtk/gtk.h>
#include <purple.h>
#include "chatty-conversation.h"

typedef struct {
  GtkStack   *stack_panes_settings;
  GtkStack   *stack_panes_new_chat;
  GtkStack   *stack_panes_muc_info;
  GtkLabel   *label_name;
  GtkLabel   *label_protocol;
  GtkLabel   *label_status;
  GtkListBox *list_select_account;
  GtkWidget  *dialog_edit_account;
  GtkWidget  *button_add_account;
  GtkWidget  *button_save_account;
  GtkWidget  *box_topic_frame;
  GtkWidget  *textview_muc_topic;
  GtkWidget  *radio_button_xmpp;
  GtkWidget  *radio_button_matrix;
  GtkWidget  *radio_button_telegram;
  const char *current_topic;
  const char *new_topic;
  GtkEntry   *entry_account_name;
  GtkEntry   *entry_account_pwd;
  GtkEntry   *entry_account_server;
  GtkEntry   *entry_contact_name;
  GtkEntry   *entry_contact_nick;
  GtkEntry   *entry_invite_name;
  GtkEntry   *entry_invite_msg;

  struct {
    GtkListBox *listbox_fp_own;
    GtkListBox *listbox_fp_own_dev;
    GtkListBox *listbox_fp_contact;
    GtkSwitch  *switch_on_off;
    GtkWidget  *label_status_msg;
  } omemo;
} chatty_dialog_data_t;

chatty_dialog_data_t *chatty_get_dialog_data(void);

GtkWidget *chatty_dialogs_create_dialog_settings (void);
GtkWidget *chatty_dialogs_create_dialog_new_chat (void);
GtkWidget *chatty_dialogs_create_dialog_muc_info (void);
void chatty_dialogs_show_dialog_new_contact (void);
void chatty_dialogs_show_dialog_join_muc (void);
void chatty_dialogs_show_dialog_user_info (ChattyConversation *chatty_conv);
void chatty_dialogs_show_dialog_about_chatty (const char *version);

#endif
