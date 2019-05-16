/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef __HISTORY_H_INCLUDE__
#define __HISTORY_H_INCLUDE__

#include "time.h"

int chatty_history_open(void);
void chatty_history_close(void);
void chatty_history_add_message(int conv_id, const char* stanza, int direction, const char* jid, const char* uid, time_t m_time);
void chatty_history_get_messages(void);

#endif
