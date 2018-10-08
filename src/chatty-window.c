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


static GtkHeaderBar *
chatty_window_set_header_bar ()
{
  GtkHeaderBar    *header_bar;
  GtkImage        *image;

  chatty_data_t *chatty = chatty_get_data ();

  header_bar = gtk_header_bar_new ();
  chatty->header_title = gtk_label_new ("");
  chatty->header_icon = gtk_image_new ();
  gtk_header_bar_set_custom_title (header_bar, chatty->header_title);
  g_object_set (header_bar,
                "spacing", 24,
                NULL);

  chatty->header_button_left = gtk_button_new ();
  gtk_widget_set_valign (chatty->header_button_left, GTK_ALIGN_CENTER);

  chatty->header_button_right = gtk_menu_button_new ();
  gtk_widget_set_valign (chatty->header_button_right, GTK_ALIGN_CENTER);

  gtk_header_bar_pack_start (header_bar, chatty->header_button_left);
  gtk_header_bar_pack_start (header_bar, chatty->header_icon);
  gtk_header_bar_pack_end (header_bar, chatty->header_button_right);

  g_signal_connect_object (chatty->header_button_left, "clicked",
                           G_CALLBACK(cb_header_bar_button_left_clicked),
                           NULL, 0);

  return header_bar;
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
  GtkWidget          *window;
  GtkBox             *vbox;
  HdyLeaflet         *hdy_leaflet;

  chatty_data_t *chatty = chatty_get_data ();

  window = gtk_application_window_new (app);
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

  gtk_window_set_titlebar (GTK_WINDOW (window),
                           chatty_window_set_header_bar ());

  chatty->panes_stack = gtk_stack_new ();

  chatty->pane_view_message_list = gtk_notebook_new ();
  chatty->pane_view_manage_account = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  chatty->pane_view_select_account = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  chatty->pane_view_new_account = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  chatty->pane_view_new_conversation = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  chatty->pane_view_buddy_list = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_stack_add_named (chatty->panes_stack, chatty->pane_view_manage_account, "view-manage-account");
  gtk_stack_add_named (chatty->panes_stack, chatty->pane_view_select_account, "view-select-account");
  gtk_stack_add_named (chatty->panes_stack, chatty->pane_view_new_conversation, "view-new-chat");
  gtk_stack_add_named (chatty->panes_stack, chatty->pane_view_new_account, "view-new-account");
  gtk_stack_add_named (chatty->panes_stack, chatty->pane_view_buddy_list, "view-chat-list");
  gtk_stack_add_named (chatty->panes_stack, chatty->pane_view_message_list, "view-message-list");

  gtk_container_add (GTK_CONTAINER (window), chatty->panes_stack);
  gtk_widget_show_all (window);
  chatty_window_init_data ();
}
