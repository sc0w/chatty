/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __PURPLE_INIT_H_INCLUDE__
#define __PURPLE_INIT_H_INCLUDE__

#include "purple.h"

#define CHATTY_UI           "chatty-ui"
#define CHATTY_PREFS_ROOT   "/chatty"

#define PURPLE_GLIB_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define PURPLE_GLIB_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

typedef struct {
  PurpleConversation *conv;
  PurpleAccount      *account;
} chatty_purple_data_t;

chatty_purple_data_t *chatty_get_purple_data(void);

void libpurple_start (void);

#endif