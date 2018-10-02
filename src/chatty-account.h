/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __ACCOUNT_H_INCLUDE__
#define __ACCOUNT_H_INCLUDE__



typedef struct
{
  GtkButton         *button_delete;
  GtkSwitch         *switch_on_off;
  GtkListBox        *list;
  GtkScrolledWindow *scroll;
} chatty_account_data_t;


chatty_account_data_t *chatty_get_account_data(void);

PurpleAccountUiOps *chatty_accounts_get_ui_ops(void);

void chatty_account_init (void);
void chatty_account_uninit (void);
void chatty_account_connect (const char *account_name, const char *account_pwd);

#endif
