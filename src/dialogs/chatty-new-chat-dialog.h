/*
 * Copyright (C) 2020 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#pragma once

#define HANDY_USE_UNSTABLE_API
#include <handy.h>

G_BEGIN_DECLS

#define CHATTY_TYPE_NEW_CHAT_DIALOG (chatty_new_chat_dialog_get_type())
G_DECLARE_FINAL_TYPE (ChattyNewChatDialog, chatty_new_chat_dialog, CHATTY, NEW_CHAT_DIALOG, HdyDialog)


GtkWidget *chatty_new_chat_dialog_new (GtkWindow *parent_window);
GtkWidget *chatty_new_chat_get_list_contacts (ChattyNewChatDialog *self);
GtkWidget *chatty_new_chat_get_search_entry (ChattyNewChatDialog *self);
void chatty_new_chat_set_edit_mode (ChattyNewChatDialog *self, gboolean edit);


G_END_DECLS
