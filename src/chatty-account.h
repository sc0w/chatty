/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __ACCOUNT_H_INCLUDE__
#define __ACCOUNT_H_INCLUDE__


PurpleAccountUiOps *chatty_accounts_get_ui_ops(void);

void chatty_account_init (void);
void chatty_account_uninit (void);
void chatty_account_connect (const char *account_name, const char *account_pwd);

#endif
