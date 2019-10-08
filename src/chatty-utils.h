/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __UTILS_H_INCLUDE__
#define __UTILS_H_INCLUDE__

#include <stdio.h>

#define MAX_GMT_ISO_SIZE 256

typedef enum {
  CHATTY_UTILS_TIME_AGO_VERBOSE    =  1 << 0,
  CHATTY_UTILS_TIME_AGO_SHOW_DATE  =  1 << 1
} ChattyTimeAgoFlags;

char *chatty_utils_jabber_id_strip (const char *name);

void chatty_utils_generate_uuid (char **uuid);

char *chatty_utils_time_ago_in_words (time_t time_stamp, ChattyTimeAgoFlags flags);

#endif