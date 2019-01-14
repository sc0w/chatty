/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __ACCOUNT_H_INCLUDE__
#define __ACCOUNT_H_INCLUDE__

#define HANDY_USE_UNSTABLE_API

typedef struct
{
  GtkListBox    *list_select;
  GtkWidget     *dialog_edit_account;
  GtkWidget     *button_add_account;
  GtkWidget     *button_set_account;
  GtkEntry      *entry_account_name;
  GtkEntry      *entry_account_pwd;
} chatty_account_data_t;


enum {
  LIST_MANAGE_ACCOUNT,
  LIST_SELECT_MUC_ACCOUNT,
  LIST_SELECT_CHAT_ACCOUNT
} e_account_list_type;


chatty_account_data_t *chatty_get_account_data(void);

PurpleAccountUiOps *chatty_accounts_get_ui_ops(void);

void *chatty_account_get_handle (void);
void chatty_account_init (void);
void chatty_account_uninit (void);
void chatty_account_add_sms_account (void);
void chatty_account_create_account_select_list (void);
void chatty_account_connect (const char *account_name,
                             const char *account_pwd);

#endif
