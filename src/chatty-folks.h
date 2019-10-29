/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __FOLKS_H_INCLUDE__
#define __FOLKS_H_INCLUDE__

#include <folks/folks.h>


typedef struct {
  FolksIndividualAggregator *aggregator;
  //GHashTable                *individuals;
  GeeMap                    *individuals;
  GtkListBox                *listbox;
  GList                     *rows;
  int                        mode;
} chatty_folks_data_t;

chatty_folks_data_t *chatty_get_folks_data(void);


enum {
  CHATTY_FOLKS_SET_CONTACT_ROW_ICON,
  CHATTY_FOLKS_SET_PURPLE_BUDDY_ICON
} e_folks_modes;


void chatty_folks_init (GtkListBox *list);
void chatty_folks_close (void);
const char *chatty_folks_has_individual_with_name (const char *name);
const char *chatty_folks_has_individual_with_phonenumber (const char *number);
const char *chatty_folks_get_individual_name_by_id (const char *id);
void chatty_folks_set_purple_buddy_avatar (const char    *folks_id, 
                                           PurpleAccount *account,
                                           const char    *user_name);

#endif
