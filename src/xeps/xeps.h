/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __XEPS_H_INCLUDE__
#define __XEPS_H_INCLUDE__

#include <libpurple/plugin.h>

void * chatty_xeps_get_handle (void);
PurplePlugin * chatty_xeps_get_jabber (void);

void chatty_xeps_init (void);
void chatty_xeps_close (void);

#endif
