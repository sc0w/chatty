/*
 * Copyright (C) 2019 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __FOLKS_H_INCLUDE__
#define __FOLKS_H_INCLUDE__

#include <folks/folks.h>

#include "users/chatty-contact.h"

typedef enum {
  CHATTY_FOLKS_SET_CONTACT_ROW_ICON,
  CHATTY_FOLKS_SET_PURPLE_BUDDY_ICON
} e_folks_modes;


void chatty_folks_set_purple_buddy_data (ChattyContact *contact,
                                         PurpleAccount *account,
                                         const char    *user_name);
void chatty_folks_load_avatar (FolksIndividual  *individual,
                               ChattyContactRow *row,
                               PurpleAccount    *account,
                               const char       *user_name,
                               int               mode,
                               int               size);

#endif
