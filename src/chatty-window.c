/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include <glib.h>
#include <glib/gi18n.h>
#include "chatty-config.h"
#include "chatty-window.h"
#include "chatty-message-list.h"
#include "chatty-buddy-list.h"
#include "chatty-purple-init.h"
#include "chatty-icons.h"
#include "chatty-popover-actions.h"
#define HANDY_USE_UNSTABLE_API
#include <handy.h>

static chatty_data_t chatty_data;


static void chatty_back_action (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data);


static const GActionEntry window_action_entries [] = {
  { "add", chatty_back_action },
  { "back", chatty_back_action },
};


chatty_data_t *chatty_get_data (void)
{
  return &chatty_data;
}


static void
chatty_empty_container (GtkContainer *container) {
  gtk_container_foreach (container, (GtkCallback)gtk_widget_destroy, NULL);
}


static void
chatty_back_action (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  ChattyWindowState state_last;

  chatty_data_t *chatty = chatty_get_data ();

  state_last = chatty->view_state_last;

  chatty_window_change_view (chatty->view_state_next);

  switch (state_last) {
    case CHATTY_VIEW_MANAGE_ACCOUNT_LIST:
      chatty_blist_refresh (purple_get_blist(), FALSE);
      break;
    case CHATTY_VIEW_NEW_ACCOUNT:
      chatty_empty_container (GTK_CONTAINER(chatty->pane_view_new_account));
      break;
    case CHATTY_VIEW_NEW_CONVERSATION:
      chatty_empty_container (GTK_CONTAINER(chatty->pane_view_new_conversation));
      break;
    case CHATTY_VIEW_MESSAGE_LIST:
    case CHATTY_VIEW_SELECT_ACCOUNT_LIST:
    case CHATTY_VIEW_CONVERSATIONS_LIST:
    default:
      break;
  }

  gtk_image_clear (GTK_IMAGE (chatty->header_icon));
}


void
chatty_window_change_view (ChattyWindowState view)
{
  gchar           *stack_id;

  chatty_data_t *chatty = chatty_get_data ();

  switch (view) {
    case CHATTY_VIEW_MANAGE_ACCOUNT_LIST:
      stack_id = "view-manage-account";
      chatty->view_state_next = CHATTY_VIEW_CONVERSATIONS_LIST;
      break;

    case CHATTY_VIEW_NEW_ACCOUNT:
      stack_id = "view-new-account";
      chatty->view_state_next = CHATTY_VIEW_MANAGE_ACCOUNT_LIST;
      break;

    case CHATTY_VIEW_SELECT_ACCOUNT_LIST:
      stack_id = "view-select-account";
      chatty->view_state_next = CHATTY_VIEW_CONVERSATIONS_LIST;
      break;

    case CHATTY_VIEW_NEW_CONVERSATION:
      stack_id = "view-new-chat";
      chatty->view_state_next = CHATTY_VIEW_SELECT_ACCOUNT_LIST;
      break;

    case CHATTY_VIEW_MESSAGE_LIST:
      stack_id = "view-message-list";
      chatty->view_state_next = CHATTY_VIEW_CONVERSATIONS_LIST;
      break;

    case CHATTY_VIEW_CONVERSATIONS_LIST:
      stack_id = "view-chat-list";
      chatty->view_state_next = CHATTY_VIEW_SELECT_ACCOUNT_LIST;
      break;

    default:
      break;
  }

  chatty->view_state_last = view;

  gtk_stack_set_visible_child_name (chatty->panes_stack, stack_id);
}


void
chatty_window_set_header_title (const char *title)
{
  chatty_data_t *chatty = chatty_get_data ();

  gtk_header_bar_set_title (chatty->header_view_message_list, title);
}


static void
chatty_window_init_data (void)
{
  chatty_window_change_view (CHATTY_VIEW_CONVERSATIONS_LIST);
  libpurple_start ();
}


void
chatty_window_activate (GtkApplication  *app,
                        gpointer        user_data)
{
  GtkBuilder         *builder;
  GtkWindow          *window;
  GSimpleActionGroup *simple_action_group;
  GtkCssProvider *cssProvider = gtk_css_provider_new();

  chatty_data_t *chatty = chatty_get_data ();

  builder = gtk_builder_new_from_resource ("/sm/puri/chatty/ui/chatty-window.ui");

  window = GTK_WINDOW (gtk_builder_get_object (builder, "window"));
  g_object_set (window, "application", app, NULL);

  simple_action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   window_action_entries,
                                   G_N_ELEMENTS (window_action_entries),
                                   window);
  gtk_widget_insert_action_group (GTK_WIDGET (window),
                                  "win",
                                  G_ACTION_GROUP (simple_action_group));

  gtk_window_set_title (GTK_WINDOW (window), "Window");
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 640);

  chatty_popover_actions_init (window);

  gtk_css_provider_load_from_resource (cssProvider,
                                       "/sm/puri/chatty/css/style.css");
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default(),
                                             GTK_STYLE_PROVIDER (cssProvider),
                                             GTK_STYLE_PROVIDER_PRIORITY_USER);

  chatty->header_view_message_list = GTK_HEADER_BAR (gtk_builder_get_object (builder, "header_view_message_list"));
  chatty->header_icon = GTK_WIDGET (gtk_builder_get_object (builder, "header_icon"));
  chatty->panes_stack = GTK_STACK (gtk_builder_get_object (builder, "panes_stack"));
  chatty->pane_view_message_list = GTK_WIDGET (gtk_builder_get_object (builder, "pane_view_message_list"));
  chatty->pane_view_manage_account = GTK_BOX (gtk_builder_get_object (builder, "pane_view_manage_account"));
  chatty->pane_view_select_account = GTK_BOX (gtk_builder_get_object (builder, "pane_view_select_account"));
  chatty->pane_view_new_account = GTK_BOX (gtk_builder_get_object (builder, "pane_view_new_account"));
  chatty->pane_view_new_conversation = GTK_BOX (gtk_builder_get_object (builder, "pane_view_new_conversation"));
  chatty->pane_view_buddy_list = GTK_BOX (gtk_builder_get_object (builder, "pane_view_buddy_list"));

  gtk_widget_show_all (GTK_WIDGET (window));
  chatty_window_init_data ();
}