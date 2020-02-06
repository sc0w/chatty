/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include <glib.h>
#include <glib/gi18n.h>
#include "chatty-popover-actions.h"
#include "chatty-window.h"
#include "chatty-utils.h"
#include "chatty-buddy-list.h"


static void
view_msg_list_cmd_delete (GSimpleAction *action,
                          GVariant      *parameter,
                          gpointer       user_data)
{
  chatty_blist_chat_list_remove_buddy ();
}


static void
view_msg_list_cmd_leave (GSimpleAction *action,
                          GVariant      *parameter,
                          gpointer       user_data)
{
  chatty_blist_chat_list_leave_chat ();
}

static void
view_msg_list_cmd_add_contact (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       user_data)
{
  chatty_blist_contact_list_add_buddy ();
}

static void
view_msg_list_cmd_add_gnome_contact (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       user_data)
{
  chatty_blist_gnome_contacts_add_buddy ();
}

static void
view_msg_list_cmd_chat_info (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  ChattyWindow *window;

  window = chatty_utils_get_window ();

  chatty_window_change_view (window, CHATTY_VIEW_CHAT_INFO);
}


static const GActionEntry view_msg_list_entries [] =
{
  { "add-contact", view_msg_list_cmd_add_contact },
  { "add-gnome-contact", view_msg_list_cmd_add_gnome_contact },
  { "leave-chat", view_msg_list_cmd_leave },
  { "delete-chat", view_msg_list_cmd_delete },
  { "chat-info", view_msg_list_cmd_chat_info }
};


void
chatty_popover_actions_init (GtkWindow *window)
{
  GSimpleActionGroup *simple_action_group;

  simple_action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   view_msg_list_entries,
                                   G_N_ELEMENTS (view_msg_list_entries),
                                   window);

  gtk_widget_insert_action_group (GTK_WIDGET (window),
                                  "msg_view",
                                  G_ACTION_GROUP (simple_action_group));
}
