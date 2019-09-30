/*
 * Copyright (C) 2019 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * 
 * Author: Julian Sparber <julian@sparber.net>
 */

#ifndef __CONTACT_ROW_H_INCLUDE__
#define __CONTACT_ROW_H_INCLUDE__

#include <gtk/gtk.h>

#define CHATTY_TYPE_CONTACT_ROW (chatty_contact_row_get_type ())
G_DECLARE_FINAL_TYPE (ChattyContactRow, chatty_contact_row, CHATTY, CONTACT_ROW, GtkListBoxRow)

    GtkWidget *chatty_contact_row_new (gpointer data,
                                       GdkPixbuf *avatar,
                                       const gchar *name,
                                       const gchar *description,
                                       const gchar *timestamp,
                                       const gchar *message_count);
#endif
