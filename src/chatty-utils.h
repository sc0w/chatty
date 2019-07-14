/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __UTILS_H_INCLUDE__
#define __UTILS_H_INCLUDE__

#include <stdio.h>

#define MAX_GMT_ISO_SIZE 256

char *chatty_utils_jabber_id_strip (const char *name);

void chatty_utils_generate_uuid (char **uuid);

#endif
