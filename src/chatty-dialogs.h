/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __DIALOGS_H_INCLUDE__
#define __DIALOGS_H_INCLUDE__

#include <gtk/gtk.h>
#include <purple.h>

typedef struct {
  GtkStack          *stack_panes_settings;
  GtkStack          *stack_panes_new_chat;
  GtkStack          *stack_panes_muc_info;
  GtkLabel          *label_name;
  GtkLabel          *label_protocol;
  GtkLabel          *label_status;
  GtkListBox        *list_select_account;
  GtkWidget         *dialog_edit_account;
  GtkWidget         *button_add_account;
  GtkWidget         *button_save_account;
  GtkWidget         *box_topic_frame;
  GtkWidget         *textview_muc_topic;
  const char        *current_topic;
  const char        *new_topic;
  GtkEntry          *entry_account_name;
  GtkEntry          *entry_account_pwd;
  GtkEntry          *entry_contact_name;
  GtkEntry          *entry_contact_nick;
  GtkEntry          *entry_invite_name;
  GtkEntry          *entry_invite_msg;
} chatty_dialog_data_t;

chatty_dialog_data_t *chatty_get_dialog_data(void);

GtkWidget * chatty_dialogs_create_dialog_settings (void);
GtkWidget * chatty_dialogs_create_dialog_new_chat (void);
GtkWidget * chatty_dialogs_create_dialog_muc_info (void);
void chatty_dialogs_show_dialog_new_contact (void);
void chatty_dialogs_show_dialog_join_muc (void);
void chatty_dialogs_show_dialog_welcome (gboolean show_label_sms);
void chatty_dialogs_show_dialog_about_chatty (const char *version);

#endif
