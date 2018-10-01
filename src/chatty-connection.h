/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */


#ifndef __CONNECTION_H_INCLUDE__
#define __CONNECTION_H_INCLUDE__

#define INITIAL_RECON_DELAY_MIN  8000
#define INITIAL_RECON_DELAY_MAX  60000

#define MAX_RECON_DELAY    600000


typedef struct {
   int delay;
   guint timeout;
} ChattyAutoRecon;


PurpleConnectionUiOps *chatty_connection_get_ui_ops (void);

void *chatty_connection_get_handle (void);

void chatty_connection_init (void);
void chatty_connection_uninit (void);

#endif
