/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include <glib.h>
#include <glib/gi18n.h>
#include "chatty-popover-actions.h"
#include "chatty-window.h"


static void
view_msg_list_cmd_call (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{

}


static void
view_blist_cmd_show_in_contacts (GSimpleAction *action,
                                 GVariant      *parameter,
                                 gpointer       user_data)
{

}


static void
view_blist_cmd_delete_conversation (GSimpleAction *action,
                                    GVariant      *parameter,
                                    gpointer       user_data)
{

}


static void
view_blist_cmd_find (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{

}


static void
view_blist_cmd_accounts (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       user_data)
{
  chatty_window_change_view (CHATTY_VIEW_MANAGE_ACCOUNT_LIST);
}


static void
view_blist_cmd_preferences (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{

}


static const GActionEntry view_blist_entries [] =
{
  //{ "find",         view_blist_cmd_find },
  { "accounts",     view_blist_cmd_accounts },
  //{ "preferences",  view_blist_cmd_preferences },
};


static const GActionEntry view_msg_list_entries [] =
{
  //{ "call",             view_msg_list_cmd_call },
  //{ "show-in-contacts", view_blist_cmd_show_in_contacts },
  //{ "delete-chat",      view_blist_cmd_delete_conversation }
};


void
chatty_popover_actions_init (GtkWindow *window)
{
  GAction            *action;
  GActionGroup       *action_group;
  GSimpleActionGroup *simple_action_group;

  simple_action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   view_blist_entries,
                                   G_N_ELEMENTS (view_blist_entries),
                                   window);

  gtk_widget_insert_action_group (GTK_WIDGET (window),
                                  "blist",
                                  G_ACTION_GROUP (simple_action_group));

  simple_action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   view_msg_list_entries,
                                   G_N_ELEMENTS (view_msg_list_entries),
                                   window);

  gtk_widget_insert_action_group (GTK_WIDGET (window),
                                  "msg_view",
                                  G_ACTION_GROUP (simple_action_group));
}
