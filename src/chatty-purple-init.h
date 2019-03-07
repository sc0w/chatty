/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __PURPLE_INIT_H_INCLUDE__
#define __PURPLE_INIT_H_INCLUDE__

#include "purple.h"

#define CHATTY_APP_NAME     "Chatty"
#define CHATTY_APP_ID       "sm.puri.Chatty"
#define CHATTY_VERSION      "v0.0.6"
#define CHATTY_UI           "chatty-ui"
#define CHATTY_PREFS_ROOT   "/chatty"

#define PURPLE_GLIB_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define PURPLE_GLIB_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

typedef struct {
  gboolean plugin_carbons_loaded;
  gboolean plugin_carbons_available;
  gboolean plugin_lurch_loaded;
  gboolean plugin_mm_sms_loaded;
} chatty_purple_data_t;

chatty_purple_data_t *chatty_get_purple_data(void);

void libpurple_start (void);
gboolean chatty_purple_unload_plugin (const char *name);
gboolean chatty_purple_load_plugin (const char *name);

#endif
