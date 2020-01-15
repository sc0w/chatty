/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __ACCOUNT_H_INCLUDE__
#define __ACCOUNT_H_INCLUDE__

#define HANDY_USE_UNSTABLE_API

enum {
  LIST_SELECT_MUC_ACCOUNT,
  LIST_SELECT_CHAT_ACCOUNT
} e_account_list_type;


PurpleAccountUiOps *chatty_accounts_get_ui_ops(void);

void *chatty_account_get_handle (void);
void chatty_account_init (void);
void chatty_account_uninit (void);
void chatty_account_create_account_select_list (void);
void chatty_account_connect (const char *account_name, const char *account_pwd);
gboolean chatty_account_populate_account_list (GtkListBox *list, guint type);

#endif
