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
  GtkStack   *stack_panes_muc_info;
  GtkLabel   *label_libremone_hint;
  GtkWidget  *box_topic_frame;
  GtkWidget  *textview_muc_topic;
  const char *current_topic;
  const char *new_topic;
  GtkEntry   *entry_name;
  GtkEntry   *entry_invite_name;
  GtkEntry   *entry_invite_msg;

  struct {
    GtkListBox *listbox_fp_own;
    GtkListBox *listbox_fp_own_dev;
  } omemo;
} chatty_dialog_data_t;

chatty_dialog_data_t *chatty_get_dialog_data(void);

GtkWidget *chatty_dialogs_create_dialog_muc_info (void);
char *chatty_dialogs_show_dialog_load_avatar (void);
void chatty_dialogs_show_dialog_about_chatty (void);

#endif
