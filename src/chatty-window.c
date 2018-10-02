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

static chatty_data_t chatty_data;

chatty_data_t *chatty_get_data (void)
{
  return &chatty_data;
}


static void
cb_header_bar_button_left_clicked (GtkButton *sender,
                                   gpointer  data)
{
  chatty_data_t *chatty = chatty_get_data ();

  if (chatty->view_state == CHATTY_VIEW_CHAT_LIST_SLIDE_RIGHT ||
    chatty->view_state == CHATTY_VIEW_CHAT_LIST_SLIDE_DOWN) {
    chatty_window_change_view (CHATTY_VIEW_NEW_CHAT);
  } else {
    chatty_window_change_view (chatty->view_state_next);
  }

  gtk_image_clear (chatty->header_icon);
}


static void
cb_button_new_chat_clicked (GtkButton *sender,
                            gpointer  data)
{
  chatty_blist_add_buddy ();
  chatty_window_change_view (CHATTY_VIEW_CHAT_LIST_SLIDE_DOWN);
}


static void
cb_button_connect_clicked (GtkButton *sender,
                           gpointer  data)
{
  gchar *name, *pwd;

  chatty_data_t *chatty = chatty_get_data();

  name = gtk_entry_get_text (GTK_ENTRY(chatty->entry_account_name));
  pwd  = gtk_entry_get_text (GTK_ENTRY(chatty->entry_account_pwd));

  if (chatty->purple_state == CHATTY_PURPLE_DISCONNECTED) {
    libpurple_start ();
    chatty_account_connect (name, pwd);
  }
}


static void
cb_buddy_name_insert_text (GtkEntry *entry,
                           const    gchar *text,
                           gint     length,
                           gint     *position,
                           gpointer data)
{
  chatty_data_t *chatty = chatty_get_data();

  // TODO validate input
  if (length) {
    gtk_widget_set_sensitive (chatty->button_add_buddy, TRUE);
  } else {
    gtk_widget_set_sensitive (chatty->button_add_buddy, FALSE);
  }
}


static void
cb_account_name_insert_text (GtkEntry *entry,
                             const    gchar *text,
                             gint     length,
                             gint     *position,
                             gpointer data)
{
  chatty_data_t *chatty = chatty_get_data();

  // TODO validate input
  if (length) {
    gtk_widget_set_sensitive (chatty->button_connect, TRUE);
  } else {
    gtk_widget_set_sensitive (chatty->button_connect, FALSE);
  }
}


void
chatty_window_clear_entries (void) {
  chatty_data_t *chatty = chatty_get_data ();

  gtk_entry_set_text (GTK_ENTRY(chatty->entry_buddy_name), "");
  gtk_entry_set_text (GTK_ENTRY(chatty->entry_buddy_nick), "");
  gtk_entry_set_text (GTK_ENTRY(chatty->entry_invite_msg), "");
}


void
chatty_window_change_view (guint view)
{
  GtkImage        *image;
  gint            type;
  gchar           *stack_id;
  gchar           *icon_button_left;
  gchar           *icon_button_right;
  gchar           *popover_id;
  gchar           *popover_path;
  GtkBuilder      *builder;
  GtkWidget       *menu_popover;

  chatty_data_t *chatty = chatty_get_data ();

  chatty->view_state = view;

  switch (view) {
    case CHATTY_VIEW_LOGIN:
      type = GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT;
      stack_id = "view-login";
      icon_button_left = "list-add-symbolic";
      icon_button_right = "open-menu-symbolic";
      chatty_window_set_header_title (_("Login"));
      popover_id = "chatty-message-view-popover";
      chatty->view_state_next = CHATTY_VIEW_CHAT_LIST_SLIDE_RIGHT;
      break;

    case CHATTY_VIEW_CHAT_LIST_SLIDE_RIGHT:
      type = GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT;
      stack_id = "view-chat-list";
      icon_button_left = "list-add-symbolic";
      icon_button_right = "open-menu-symbolic";
      popover_id = "chatty-blist-view-popover";
      chatty_window_set_header_title (_("Conversations"));
      chatty->view_state_next = CHATTY_VIEW_MESSAGE_LIST;
      break;

    case CHATTY_VIEW_CHAT_LIST_SLIDE_DOWN:
      type = GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN;
      stack_id = "view-chat-list";
      icon_button_left = "list-add-symbolic";
      icon_button_right = "open-menu-symbolic";
      popover_id = "chatty-blist-view-popover";
      chatty_window_set_header_title (_("Conversations"));
      chatty->view_state_next = CHATTY_VIEW_MESSAGE_LIST;
      chatty_window_clear_entries();
      break;

    case CHATTY_VIEW_MESSAGE_LIST:
      type = GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT;
      stack_id = "view-message-list";
      icon_button_left = "go-previous-symbolic";
      icon_button_right = "view-more-symbolic";
      popover_id = "chatty-message-view-popover";
      chatty->view_state_next = CHATTY_VIEW_CHAT_LIST_SLIDE_RIGHT;
      break;

    case CHATTY_VIEW_NEW_CHAT:
      type = GTK_STACK_TRANSITION_TYPE_SLIDE_UP;
      stack_id = "view-new-chat";
      icon_button_left = "go-down-symbolic";
      icon_button_right = "view-more-symbolic";
      popover_id = "chatty-new-chat-view-popover";
      chatty_window_set_header_title (_("Add new chat"));
      chatty->view_state_next = CHATTY_VIEW_CHAT_LIST_SLIDE_DOWN;
      break;
  }

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

  g_free (popover_path);
  g_object_unref (builder);

  gtk_stack_set_transition_type (chatty->panes_stack, type);
  gtk_stack_set_visible_child_name (chatty->panes_stack, stack_id);
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
  GtkStyleContext *sc;

  chatty_data_t *chatty = chatty_get_data ();

  header_bar = gtk_header_bar_new ();
  chatty->header_title = gtk_label_new ("");
  chatty->header_icon = gtk_image_new ();
  gtk_header_bar_set_custom_title (header_bar, chatty->header_title);

  chatty->header_button_left = gtk_button_new ();
  gtk_widget_set_valign (chatty->header_button_left, GTK_ALIGN_CENTER);
  sc = gtk_widget_get_style_context (chatty->header_button_left);
  gtk_style_context_add_class (sc, "button_left");

  chatty->header_button_right = gtk_menu_button_new ();
  gtk_widget_set_valign (chatty->header_button_right, GTK_ALIGN_CENTER);
  sc = gtk_widget_get_style_context (chatty->header_button_right);
  gtk_style_context_add_class (sc, "button_right");

  g_object_set (header_bar, "spacing", 24, NULL);

  gtk_header_bar_pack_start (header_bar, chatty->header_button_left);
  gtk_header_bar_pack_start (header_bar, chatty->header_icon);
  gtk_header_bar_pack_end (header_bar, chatty->header_button_right);

  g_signal_connect_object (chatty->header_button_left, "clicked",
                           G_CALLBACK(cb_header_bar_button_left_clicked),
                           NULL, 0);

  return header_bar;
}


static GtkGrid *
chatty_window_login_grid()
{
  GtkWidget *grid;
  GtkWidget *label;

  chatty_data_t *chatty = chatty_get_data ();

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 20);

  label = gtk_label_new ("XMPP account login");
  gtk_grid_attach (GTK_GRID (grid), label, 1, 1, 1, 1);

  chatty->entry_account_name = gtk_entry_new ();
  gtk_entry_set_placeholder_text (GTK_ENTRY (chatty->entry_account_name), _("Username"));
  gtk_entry_set_visibility (GTK_ENTRY (chatty->entry_account_name), 1);
  gtk_grid_attach (GTK_GRID (grid), chatty->entry_account_name, 1, 2, 1, 1);

  g_signal_connect (G_OBJECT(chatty->entry_account_name),
                    "insert_text",
                    G_CALLBACK(cb_account_name_insert_text),
                    NULL);

  chatty->entry_account_pwd = gtk_entry_new ();
  gtk_entry_set_placeholder_text (GTK_ENTRY (chatty->entry_account_pwd), _("Password"));
  gtk_entry_set_visibility (GTK_ENTRY (chatty->entry_account_pwd), 0);
  gtk_grid_attach (GTK_GRID (grid), chatty->entry_account_pwd, 1, 3, 1, 1);

  chatty->button_connect = gtk_button_new_with_label (_("Connect"));
  gtk_grid_attach (GTK_GRID (grid), chatty->button_connect, 1, 4, 1, 1);

  chatty->label_status = gtk_label_new (NULL);
  gtk_grid_attach (GTK_GRID (grid), chatty->label_status, 0, 5, 4, 1);
  gtk_widget_set_sensitive (chatty->button_connect, FALSE);
  g_signal_connect_object (chatty->button_connect, "clicked",
                           G_CALLBACK (cb_button_connect_clicked),
                           NULL, 0);

  gtk_widget_set_halign (GTK_GRID (grid), GTK_ALIGN_CENTER);
  gtk_widget_set_valign (GTK_GRID (grid), GTK_ALIGN_CENTER);

  return grid;
}


static GtkGrid *
chatty_window_buddy_grid()
{
  GtkWidget       *grid;
  GtkImage        *image;
  GtkWidget       *button_avatar;
  GtkStyleContext *sc;

  chatty_data_t *chatty = chatty_get_data ();

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 20);

  button_avatar = gtk_menu_button_new ();
  gtk_widget_set_hexpand (button_avatar, FALSE);
  gtk_widget_set_halign (button_avatar, GTK_ALIGN_CENTER);
  gtk_widget_set_size_request (GTK_WIDGET(button_avatar), 80, 80);
  image = gtk_image_new_from_icon_name ("avatar-default-symbolic", GTK_ICON_SIZE_DIALOG);
  gtk_button_set_image (GTK_BUTTON (button_avatar), image);
  sc = gtk_widget_get_style_context (button_avatar);
  gtk_style_context_add_class (sc, "button_avatar");

  gtk_grid_attach (GTK_GRID (grid), button_avatar, 0, 0, 1, 1);

  chatty->entry_buddy_name = gtk_entry_new ();
  gtk_entry_set_placeholder_text (GTK_ENTRY (chatty->entry_buddy_name),
                                  "id@any-server.com");

  g_signal_connect (G_OBJECT(chatty->entry_buddy_name),
                    "insert_text",
                    G_CALLBACK(cb_buddy_name_insert_text),
                    NULL);

  gtk_grid_attach (GTK_GRID (grid), chatty->entry_buddy_name, 0, 2, 1, 1);

  chatty->entry_buddy_nick = gtk_entry_new ();
  gtk_entry_set_placeholder_text (GTK_ENTRY (chatty->entry_buddy_nick),
                                  _("Nickname (optional)"));
  gtk_grid_attach (GTK_GRID (grid), chatty->entry_buddy_nick, 0, 3, 1, 1);

  chatty->entry_invite_msg = gtk_entry_new ();
  gtk_entry_set_placeholder_text (GTK_ENTRY (chatty->entry_invite_msg),
                                  _("Invite message (optional)"));
  gtk_grid_attach (GTK_GRID (grid), chatty->entry_invite_msg, 0, 4, 1, 1);

  chatty->button_add_buddy = gtk_button_new_with_label ("Add");
  gtk_grid_attach (GTK_GRID (grid), chatty->button_add_buddy, 0, 5, 1, 1);
  gtk_widget_set_sensitive (chatty->button_add_buddy, FALSE);
  g_signal_connect_object (chatty->button_add_buddy, "clicked",
                           G_CALLBACK (cb_button_new_chat_clicked),
                           NULL, 0);

  gtk_widget_set_halign (GTK_GRID (grid), GTK_ALIGN_CENTER);
  gtk_widget_set_valign (GTK_GRID (grid), GTK_ALIGN_CENTER);

  return grid;
}


static void
chatty_window_init_data ()
{
  chatty_data_t *chatty = chatty_get_data();

  chatty->purple_state = CHATTY_PURPLE_DISCONNECTED;
  chatty->view_state = CHATTY_VIEW_LOGIN;
  chatty->view_state_next = CHATTY_VIEW_CHAT_LIST_SLIDE_RIGHT;

  chatty_window_set_header_title (_("Login"));

  gtk_widget_hide (chatty->header_button_left);
  gtk_widget_hide (chatty->header_button_right);
}


void
chatty_window_activate (GtkApplication  *app,
                        gpointer        user_data)
{
  GtkWidget   *window;
  GtkBox      *vbox;

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

  GtkCssProvider *cssProvider = gtk_css_provider_new();
  gtk_css_provider_load_from_resource (cssProvider,
                                       "/sm/puri/chatty/css/style.css");
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default(),
                                             GTK_STYLE_PROVIDER (cssProvider),
                                             GTK_STYLE_PROVIDER_PRIORITY_USER);

  gtk_window_set_titlebar (GTK_WINDOW (window),
                           chatty_window_set_header_bar ());

  chatty->conv_notebook = gtk_notebook_new ();
  chatty->blist_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  chatty->panes_stack = gtk_stack_new ();

  gtk_stack_add_named (chatty->panes_stack, chatty_window_login_grid (), "view-login");
  gtk_stack_add_named (chatty->panes_stack, chatty_window_buddy_grid (), "view-new-chat");
  gtk_stack_add_named (chatty->panes_stack, chatty->blist_box, "view-chat-list");
  gtk_stack_add_named (chatty->panes_stack, chatty->conv_notebook, "view-message-list");

  gtk_container_add (GTK_CONTAINER (window), chatty->panes_stack);
  gtk_widget_show_all (window);
  chatty_window_init_data ();
}
