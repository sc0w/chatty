/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __CONNECTION_H_INCLUDE__
#define __CONNECTION_H_INCLUDE__


typedef struct {
   int delay;
   guint timeout;
} ChattyAutoRecon;


PurpleConnectionUiOps *chatty_connection_get_ui_ops (void);

void *chatty_connection_get_handle (void);
void chatty_connection_init (void);
void chatty_connection_uninit (void);

#endif
