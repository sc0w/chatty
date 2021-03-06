/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-list-row.h
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

#include "users/chatty-item.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_LIST_ROW (chatty_list_row_get_type ())

G_DECLARE_FINAL_TYPE (ChattyListRow, chatty_list_row, CHATTY, LIST_ROW, GtkListBoxRow)

GtkWidget  *chatty_list_row_new      (ChattyItem    *item);
ChattyItem *chatty_list_row_get_item (ChattyListRow *self);
void        chatty_list_row_set_item (ChattyListRow *self,
                                      ChattyItem    *item);
GtkWidget  *chatty_list_contact_row_new (ChattyItem *item);


G_END_DECLS
