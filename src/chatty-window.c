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

chatty_data_t *chatty_get_data (void)
{
  return &chatty_data;
}


static void
chatty_destroy_widget (GtkWidget *widget) {
  GList *iter;
  GList *children;

  children = gtk_container_get_children (widget);

  for (iter = children; iter != NULL; iter = g_list_next (iter)) {
    gtk_widget_destroy (GTK_WIDGET(iter->data));
  }

  g_list_free (children);
  g_list_free (iter);
}


static void
cb_header_bar_button_left_clicked (GtkButton *sender,
                                   gpointer  data)
{
  guint state_last;

  chatty_data_t *chatty = chatty_get_data ();

  state_last = chatty->view_state_last;

  chatty_window_change_view (chatty->view_state_next);

  switch (state_last) {
    case CHATTY_VIEW_MANAGE_ACCOUNT_LIST:
      chatty_blist_refresh (purple_get_blist(), FALSE);
      break;
    case CHATTY_VIEW_NEW_ACCOUNT:
      chatty_destroy_widget (chatty->pane_view_new_account);
      break;
    case CHATTY_VIEW_NEW_CONVERSATION:
      chatty_destroy_widget (chatty->pane_view_new_conversation);
      break;
  }

  gtk_image_clear (chatty->header_icon);
}


void
chatty_window_change_view (guint view)
{
  GtkImage        *image;
  GtkBuilder      *builder;
  GtkWidget       *menu_popover;
  gint            type;
  gchar           *stack_id;
  gchar           *icon_button_left;
  gchar           *icon_button_right;
  gchar           *popover_id;
  gchar           *popover_path;
  gboolean        *submenu_enabled;

  chatty_data_t *chatty = chatty_get_data ();

  icon_button_left = "go-previous-symbolic";
  icon_button_right = "view-more-symbolic";
  type = GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT;

  switch (view) {
    case CHATTY_VIEW_MANAGE_ACCOUNT_LIST:
      if (chatty->view_state_last == CHATTY_VIEW_NEW_ACCOUNT) {
        type = GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT;
      }
      stack_id = "view-manage-account";
      submenu_enabled = FALSE;
      chatty_window_set_header_title (_("Manage accounts"));
      popover_id = "chatty-blist-view-popover";
      chatty->view_state_next = CHATTY_VIEW_CONVERSATIONS_LIST;
      break;

    case CHATTY_VIEW_NEW_ACCOUNT:
      stack_id = "view-new-account";
      icon_button_right = "open-menu-symbolic";
      submenu_enabled = TRUE;
      chatty_window_set_header_title (_("New account"));
      popover_id = "chatty-new-chat-view-popover";
      chatty->view_state_next = CHATTY_VIEW_MANAGE_ACCOUNT_LIST;
      break;

    case CHATTY_VIEW_SELECT_ACCOUNT_LIST:
      if (chatty->view_state_last == CHATTY_VIEW_NEW_CONVERSATION) {
        type = GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT;
      }
      stack_id = "view-select-account";
      submenu_enabled = FALSE;
      chatty_window_set_header_title (_("Select account for conversation"));
      popover_id = "chatty-blist-view-popover";
      chatty->view_state_next = CHATTY_VIEW_CONVERSATIONS_LIST;
      break;

    case CHATTY_VIEW_NEW_CONVERSATION:
      stack_id = "view-new-chat";
      popover_id = "chatty-new-chat-view-popover";
      chatty_window_set_header_title (_("Add new conversation"));
      chatty->view_state_next = CHATTY_VIEW_SELECT_ACCOUNT_LIST;
      break;

    case CHATTY_VIEW_MESSAGE_LIST:
      stack_id = "view-message-list";
      popover_id = "chatty-message-view-popover";
      chatty->view_state_next = CHATTY_VIEW_CONVERSATIONS_LIST;
      break;

    case CHATTY_VIEW_CONVERSATIONS_LIST:
      type = GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT;
      stack_id = "view-chat-list";
      icon_button_left = "list-add-symbolic";
      icon_button_right = "open-menu-symbolic";
      submenu_enabled = TRUE;
      popover_id = "chatty-blist-view-popover";
      chatty_window_set_header_title (_("Conversations"));
      chatty->view_state_next = CHATTY_VIEW_SELECT_ACCOUNT_LIST;
      break;
  }

  chatty->view_state_last = view;

  image = gtk_image_new_from_icon_name (icon_button_left, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (chatty->header_button_left), image);

  image = gtk_image_new_from_icon_name (icon_button_right, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (chatty->header_button_right), image);

  popover_path = g_strjoin (NULL,
                            "/sm/puri/chatty/ui/",
                            popover_id,
                            ".ui",
                            NULL);

  builder = gtk_builder_new_from_resource (popover_path);
  menu_popover = GTK_WIDGET (gtk_builder_get_object (builder, popover_id));

  gtk_menu_button_set_popover (GTK_MENU_BUTTON (chatty->header_button_right),
                               menu_popover);

  submenu_enabled ? gtk_widget_show (chatty->header_button_right) :
                    gtk_widget_hide (chatty->header_button_right);

  gtk_stack_set_transition_type (chatty->panes_stack, type);
  gtk_stack_set_visible_child_name (chatty->panes_stack, stack_id);

  g_free (popover_path);
  g_object_unref (builder);
}


void
chatty_window_set_header_title (const char *title)
{
  chatty_data_t *chatty = chatty_get_data ();

  gtk_label_set_text (GTK_LABEL(chatty->header_title), _(title));
}


static void
chatty_window_init_data ()
{
  chatty_data_t *chatty = chatty_get_data ();

  chatty_window_change_view (CHATTY_VIEW_CONVERSATIONS_LIST);

  libpurple_start ();
}


void
chatty_window_activate (GtkApplication  *app,
                        gpointer        user_data)
{
  GtkBuilder         *builder;
  GtkWidget          *window;
  GtkHeaderBar       *header_bar;
  GtkBox             *vbox;
  HdyLeaflet         *hdy_leaflet;

  chatty_data_t *chatty = chatty_get_data ();

  builder = gtk_builder_new_from_resource ("/sm/puri/chatty/ui/chatty-window.ui");

  window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
  g_object_set (window, "application", app, NULL);
  gtk_window_set_title (GTK_WINDOW (window), "Window");

#if defined (__arm__)
  gtk_window_maximize (GTK_WINDOW (window));
  gtk_window_get_size (GTK_WINDOW (window),
                       &chatty->window_size_x,
                       &chatty->window_size_y);
#else
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 640);
#endif

  chatty_popover_actions_init (window);

  GtkCssProvider *cssProvider = gtk_css_provider_new();
  gtk_css_provider_load_from_resource (cssProvider,
                                       "/sm/puri/chatty/css/style.css");
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default(),
                                             GTK_STYLE_PROVIDER (cssProvider),
                                             GTK_STYLE_PROVIDER_PRIORITY_USER);

  header_bar = GTK_HEADER_BAR (gtk_builder_get_object (builder, "header_bar"));
  chatty->header_title = GTK_LABEL (gtk_builder_get_object (builder, "header_title"));
  chatty->header_icon = GTK_IMAGE (gtk_builder_get_object (builder, "header_icon"));
  chatty->header_button_left = GTK_BUTTON (gtk_builder_get_object (builder, "header_button_left"));
  chatty->header_button_right = GTK_BUTTON (gtk_builder_get_object (builder, "header_button_right"));
  chatty->panes_stack = GTK_STACK (gtk_builder_get_object (builder, "panes_stack"));
  chatty->pane_view_message_list = GTK_NOTEBOOK (gtk_builder_get_object (builder, "pane_view_message_list"));
  chatty->pane_view_manage_account = GTK_BOX (gtk_builder_get_object (builder, "pane_view_manage_account"));
  chatty->pane_view_select_account = GTK_BOX (gtk_builder_get_object (builder, "pane_view_select_account"));
  chatty->pane_view_new_account = GTK_BOX (gtk_builder_get_object (builder, "pane_view_new_account"));
  chatty->pane_view_new_conversation = GTK_BOX (gtk_builder_get_object (builder, "pane_view_new_conversation"));
  chatty->pane_view_buddy_list = GTK_BOX (gtk_builder_get_object (builder, "pane_view_buddy_list"));

  g_object_set (header_bar,
                "spacing", 24,
                NULL);
  gtk_widget_set_valign (chatty->header_button_left, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (chatty->header_button_right, GTK_ALIGN_CENTER);

  g_signal_connect_object (chatty->header_button_left, "clicked",
                           G_CALLBACK(cb_header_bar_button_left_clicked),
                           NULL, 0);

  gtk_widget_show_all (window);
  chatty_window_init_data ();
}
