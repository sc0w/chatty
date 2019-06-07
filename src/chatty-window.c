/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include <glib.h>
#include <glib/gi18n.h>
#include "chatty-config.h"
#include "chatty-dialogs.h"
#include "chatty-window.h"
#include "chatty-message-list.h"
#include "chatty-buddy-list.h"
#include "chatty-conversation.h"
#include "chatty-account.h"
#include "chatty-purple-init.h"
#include "chatty-icons.h"
#include "chatty-popover-actions.h"

static chatty_data_t chatty_data;

static void chatty_update_header (void);

static void chatty_back_action (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data);

static void chatty_new_chat_action (GSimpleAction *action,
                                    GVariant      *parameter,
                                    gpointer       user_data);

static void chatty_add_contact_action (GSimpleAction *action,
                                       GVariant      *parameter,
                                       gpointer       user_data);


static const GActionEntry window_action_entries [] = {
  { "add", chatty_new_chat_action },
  { "add-contact", chatty_add_contact_action },
  { "back", chatty_back_action },
};


chatty_data_t *chatty_get_data (void)
{
  return &chatty_data;
}


static void
cb_leaflet_visible_child (GObject       *sender,
                          GParamSpec    *pspec,
                          gpointer      *data)
{
  chatty_update_header ();
}


static void
cb_leaflet_notify_fold (GObject       *sender,
                        GParamSpec    *pspec,
                        gpointer      *data)
{
  chatty_data_t *chatty = chatty_get_data ();

  HdyFold fold = hdy_leaflet_get_fold (chatty->header_box);

  chatty_blist_chat_list_selection (fold != HDY_FOLD_FOLDED);

  chatty_update_header ();
}


static void
chatty_update_header (void)
{
  chatty_data_t *chatty = chatty_get_data ();

  GtkWidget *header_child = hdy_leaflet_get_visible_child (chatty->header_box);
  HdyFold fold = hdy_leaflet_get_fold (chatty->header_box);

  g_assert (header_child == NULL || GTK_IS_HEADER_BAR (header_child));

  hdy_header_group_set_focus (chatty->header_group, fold == HDY_FOLD_FOLDED ? GTK_HEADER_BAR (header_child) : NULL);
}


static void
chatty_add_contact_action (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
  chatty_window_change_view (CHATTY_VIEW_NEW_CHAT);
}


static void
chatty_back_action (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  chatty_blist_returned_from_chat ();
  chatty_blist_refresh (purple_get_blist ());
  chatty_window_change_view (CHATTY_VIEW_CHAT_LIST);
}


static void
chatty_new_chat_action (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{
  chatty_window_change_view (CHATTY_VIEW_NEW_CHAT);
}


static void
chatty_window_show_chat_info (void)
{
  ChattyConversation *chatty_conv;

  chatty_data_t *chatty = chatty_get_data ();

  chatty_conv = chatty_conv_container_get_active_chatty_conv (GTK_NOTEBOOK(chatty->pane_view_message_list));

  if (purple_conversation_get_type (chatty_conv->conv) == PURPLE_CONV_TYPE_IM) {
    chatty_dialogs_show_dialog_user_info (chatty_conv);

  } else if (purple_conversation_get_type (chatty_conv->conv) == PURPLE_CONV_TYPE_CHAT) {
    gtk_widget_show (GTK_WIDGET(chatty->dialog_muc_info));
  }
}


void
chatty_window_change_view (ChattyWindowState view)
{
  chatty_data_t *chatty = chatty_get_data ();

  switch (view) {
    case CHATTY_VIEW_SETTINGS:
      gtk_widget_show (GTK_WIDGET(chatty->dialog_settings));
      break;
    case CHATTY_VIEW_ABOUT_CHATTY:
      chatty_dialogs_show_dialog_about_chatty (CHATTY_VERSION);
      break;
    case CHATTY_VIEW_JOIN_CHAT:
      chatty_dialogs_show_dialog_join_muc ();
      break;
    case CHATTY_VIEW_NEW_CHAT:
      gtk_widget_show (GTK_WIDGET(chatty->dialog_new_chat));
      break;
    case CHATTY_VIEW_CHAT_INFO:
      chatty_window_show_chat_info ();
      break;
    case CHATTY_VIEW_MESSAGE_LIST:
      hdy_leaflet_set_visible_child_name (chatty->content_box, "content");
      break;
    case CHATTY_VIEW_CHAT_LIST:
      hdy_leaflet_set_visible_child_name (chatty->content_box, "sidebar");
      break;
    default:
      break;
  }
}


void
chatty_window_update_sub_header_titlebar (GdkPixbuf  *icon,
                                          const char *title)
{
  chatty_data_t *chatty = chatty_get_data ();

  if (icon != NULL) {
    gtk_image_set_from_pixbuf (GTK_IMAGE(chatty->sub_header_icon), icon);
  } else {
    gtk_image_clear (GTK_IMAGE(chatty->sub_header_icon));
  }

  gtk_label_set_label (GTK_LABEL(chatty->sub_header_label), title);
}


void
chatty_window_welcome_screen_show (gboolean show)
{
  chatty_data_t *chatty = chatty_get_data ();

  if (show) {
    gtk_widget_show (GTK_WIDGET(chatty->box_welcome_overlay));
  } else {
    gtk_widget_hide (GTK_WIDGET(chatty->box_welcome_overlay));
  }

  if (purple_accounts_find ("SMS", "prpl-mm-sms")) {
    gtk_widget_show (GTK_WIDGET(chatty->label_welcome_overlay_sms));
  } else {
    gtk_widget_hide (GTK_WIDGET(chatty->label_welcome_overlay_sms));
  }
}


static void
chatty_window_init_data (void)
{
  chatty_data_t *chatty = chatty_get_data ();

  chatty->dummy_prefix_radio = gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON (NULL));

  chatty_window_change_view (CHATTY_VIEW_CHAT_LIST);

  // These dialogs need to be created before purple_blist_show()
  chatty->dialog_new_chat = chatty_dialogs_create_dialog_new_chat ();
  chatty->dialog_muc_info = chatty_dialogs_create_dialog_muc_info ();

  libpurple_init ();

  chatty->dialog_settings = chatty_dialogs_create_dialog_settings ();

  hdy_leaflet_set_visible_child_name (chatty->content_box, "sidebar");

  hdy_search_bar_connect_entry (chatty->search_bar_chats,
                                chatty->search_entry_chats);

  if (purple_prefs_get_bool (CHATTY_PREFS_ROOT "/status/first_start")) {
    chatty_window_welcome_screen_show (TRUE);
    purple_prefs_set_bool (CHATTY_PREFS_ROOT "/status/first_start", FALSE);
  }
}


void
chatty_window_activate (GtkApplication *app,
                        gpointer        user_data)
{
  GtkBuilder         *builder;
  GtkWindow          *window;
  GSimpleActionGroup *simple_action_group;

  chatty_data_t *chatty = chatty_get_data ();

  memset (chatty, 0, sizeof(chatty_data_t));

  chatty->app = app;

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

  chatty_popover_actions_init (window);

  chatty->main_window = window;

  chatty->header_spinner = GTK_WIDGET (gtk_builder_get_object (builder, "header_spinner"));
  chatty->sub_header_bar = GTK_HEADER_BAR (gtk_builder_get_object (builder, "sub_header_bar"));
  chatty->sub_header_label = GTK_WIDGET (gtk_builder_get_object (builder, "sub_header_label"));
  chatty->sub_header_icon = GTK_WIDGET (gtk_builder_get_object (builder, "sub_header_icon"));
  chatty->button_menu_add_contact = GTK_WIDGET (gtk_builder_get_object (builder, "button_menu_add_contact"));
  chatty->button_header_chat_info = GTK_WIDGET (gtk_builder_get_object (builder, "button_header_chat_info"));

  chatty->search_bar_chats = HDY_SEARCH_BAR (gtk_builder_get_object (builder, "search_bar_chats"));
  chatty->search_entry_chats = GTK_ENTRY (gtk_builder_get_object (builder, "search_entry_chats"));

  chatty->content_box = HDY_LEAFLET (gtk_builder_get_object (builder, "content_box"));
  chatty->header_box = HDY_LEAFLET (gtk_builder_get_object (builder, "header_box"));
  chatty->header_group = HDY_HEADER_GROUP (gtk_builder_get_object (builder, "header_group"));

  chatty->box_welcome_overlay = GTK_BOX (gtk_builder_get_object (builder, "welcome_overlay"));
  chatty->label_welcome_overlay_sms = GTK_WIDGET (gtk_builder_get_object (builder, "label_sms"));

  chatty->pane_view_message_list = GTK_WIDGET (gtk_builder_get_object (builder, "pane_view_message_list"));
  chatty->pane_view_chat_list = GTK_BOX (gtk_builder_get_object (builder, "pane_view_chat_list"));

  gtk_builder_add_callback_symbol (builder,
                                   "cb_leaflet_notify_fold",
                                   G_CALLBACK(cb_leaflet_notify_fold));
  gtk_builder_add_callback_symbol (builder,
                                   "cb_leaflet_visible_child",
                                   G_CALLBACK(cb_leaflet_visible_child));

  gtk_builder_connect_signals (builder, NULL);

  g_object_unref (builder);
  gtk_widget_show (GTK_WIDGET (window));
  chatty_window_init_data ();
}
