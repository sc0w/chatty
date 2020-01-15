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
#include "chatty-pp-account.h"

#define MAX_GMT_ISO_SIZE 256

typedef enum {
  CHATTY_UTILS_TIME_AGO_VERBOSE    =  1 << 0,
  CHATTY_UTILS_TIME_AGO_SHOW_DATE  =  1 << 1,
  CHATTY_UTILS_TIME_AGO_NO_MARKUP  =  1 << 2,
} ChattyTimeAgoFlags;

char *chatty_utils_jabber_id_strip (const char *name);
char *chatty_utils_strip_blanks (const char *string);
char *chatty_utils_strip_cr_lf (const char *string);
char *chatty_utils_check_phonenumber (const char *phone_number);
void chatty_utils_generate_uuid (char **uuid);
char *chatty_utils_time_ago_in_words (time_t time_stamp, ChattyTimeAgoFlags flags);
gboolean chatty_utils_get_item_position (GListModel *list,
                                         gpointer    item,
                                         guint      *position);
ChattyPpAccount *chatty_pp_account_find (PurpleAccount *account);
gboolean       chatty_pp_account_remove (ChattyPpAccount *self);

#endif
