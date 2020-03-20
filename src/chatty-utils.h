/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __UTILS_H_INCLUDE__
#define __UTILS_H_INCLUDE__

#include <stdio.h>
#include <gio/gio.h>

#include "chatty-window.h"
#include "users/chatty-pp-account.h"

#define MAX_GMT_ISO_SIZE 256

ChattyWindow *chatty_utils_get_window (void);
char *chatty_utils_jabber_id_strip (const char *name);
char *chatty_utils_strip_blanks (const char *string);
char *chatty_utils_check_phonenumber (const char *phone_number);
gboolean chatty_utils_get_item_position (GListModel *list,
                                         gpointer    item,
                                         guint      *position);
gboolean chatty_utils_remove_list_item  (GListStore *store,
                                         gpointer    item);
GtkWidget* chatty_utils_create_fingerprint_row (const char *fp, guint id);
gpointer   chatty_utils_get_node_object (PurpleBlistNode *node);

#endif
