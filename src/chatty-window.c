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

static void chatty_add_contact_action (GSimpleAction *action,
                                       GVariant      *parameter,
                                       gpointer       user_data);

static void chatty_search_action (GSimpleAction *action,
                                  GVariant      *state,
                                  gpointer       user_data);


static const GActionEntry window_action_entries [] = {
  { "add", chatty_back_action },
  { "add-contact", chatty_add_contact_action },
  { "search", NULL, NULL, "false", chatty_search_action },
  { "back", chatty_back_action },
};


chatty_data_t *chatty_get_data (void)
{
  return &chatty_data;
}


static gboolean
cb_switch_on_off_state_changed (GtkSwitch *widget,
                                gboolean   state,
                                gpointer   data)
{
  switch (GPOINTER_TO_INT(data)) {
    case CHATTY_PREF_SEND_RECEIPTS:
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/blist/show_offline_buddies", state);
      break;
    case CHATTY_PREF_CARBON_COPY:
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/blist/show_offline_buddies", state);
      break;
    case CHATTY_PREF_TYPING_NOTIFICATION:
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/conversations/send_typing", state);
      break;
    case CHATTY_PREF_SHOW_OFFLINE:
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/blist/show_offline_buddies", state);
      break;
    case CHATTY_PREF_INDICATE_OFFLINE:
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/blist/greyout_offline_buddies", state);
      break;
    case CHATTY_PREF_INDICATE_IDLE:
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/blist/blur_idle_buddies", state);
      break;
    case CHATTY_PREF_CONVERT_SMILEY:
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/conversations/convert_emoticons", state);
      break;
    case CHATTY_PREF_RETURN_SENDS:
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/blist/show_offline_buddies", state);
      break;
    default:
      break;
  }

  gtk_switch_set_state (widget, state);

  return TRUE;
}


static void
chatty_empty_container (GtkContainer *container) {
  gtk_container_foreach (container, (GtkCallback)gtk_widget_destroy, NULL);
}


static void
chatty_search_action (GSimpleAction *action,
                      GVariant      *state,
                      gpointer       user_data)
{
  gboolean active;

  chatty_data_t *chatty = chatty_get_data ();

  active = g_variant_get_boolean (state);

  hdy_search_bar_set_search_mode (HDY_SEARCH_BAR (chatty->search_bar), active);

  g_simple_action_set_state (action, g_variant_new_boolean (active));
}


static void
chatty_add_contact_action (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
  chatty_window_change_view (CHATTY_VIEW_SELECT_ACCOUNT);
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
    case CHATTY_VIEW_SETTINGS:
      chatty_blist_refresh (purple_get_blist(), FALSE);
      break;
    case CHATTY_VIEW_NEW_ACCOUNT:
      chatty_empty_container (GTK_CONTAINER(chatty->pane_view_new_account));
      break;
    case CHATTY_VIEW_ADD_CONTACT:
      chatty_empty_container (GTK_CONTAINER(chatty->pane_view_new_contact));
      break;
    case CHATTY_VIEW_MESSAGE_LIST:
      chatty_blist_returned_from_chat ();
      break;
    case CHATTY_VIEW_NEW_CHAT:
    case CHATTY_VIEW_SELECT_ACCOUNT:
    case CHATTY_VIEW_CHAT_LIST:
    default:
      break;
  }

  gtk_image_clear (GTK_IMAGE (chatty->header_icon));
}


void
chatty_window_change_view (ChattyWindowState view)
{
  gchar         *stack_id;

  chatty_data_t *chatty = chatty_get_data ();

  switch (view) {
    case CHATTY_VIEW_SETTINGS:
      stack_id = "view-settings";
      chatty->view_state_next = CHATTY_VIEW_CHAT_LIST;
      break;
    case CHATTY_VIEW_NEW_ACCOUNT:
      stack_id = "view-new-account";
      chatty->view_state_next = CHATTY_VIEW_SETTINGS;
      break;
    case CHATTY_VIEW_NEW_CHAT:
      stack_id = "view-new-chat";
      chatty->view_state_next = CHATTY_VIEW_CHAT_LIST;
      break;
    case CHATTY_VIEW_SELECT_ACCOUNT:
      stack_id = "view-select-account";
      chatty->view_state_next = CHATTY_VIEW_CHAT_LIST;
      break;
    case CHATTY_VIEW_MESSAGE_LIST:
      stack_id = "view-message-list";
      chatty->view_state_next = CHATTY_VIEW_CHAT_LIST;
      break;
    case CHATTY_VIEW_CHAT_LIST:
      stack_id = "view-chat-list";
      chatty->view_state_next = CHATTY_VIEW_NEW_CHAT;
      break;
    case CHATTY_VIEW_ADD_CONTACT:
      stack_id = "view-new-contact";
      chatty->view_state_next = CHATTY_VIEW_NEW_CHAT;
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
  chatty_data_t *chatty = chatty_get_data ();

  chatty_window_change_view (CHATTY_VIEW_CHAT_LIST);

  libpurple_start ();

  gtk_switch_set_state (chatty->prefs_switch_typing_notification,
                        purple_prefs_get_bool (CHATTY_PREFS_ROOT "/conversations/send_typing"));
  gtk_switch_set_state (chatty->prefs_switch_show_offline,
                        purple_prefs_get_bool (CHATTY_PREFS_ROOT "/blist/show_offline_buddies"));
  gtk_switch_set_state (chatty->prefs_switch_indicate_offline,
                        purple_prefs_get_bool (CHATTY_PREFS_ROOT "/blist/greyout_offline_buddies"));
  gtk_switch_set_state (chatty->prefs_switch_indicate_idle,
                        purple_prefs_get_bool (CHATTY_PREFS_ROOT "/blist/blur_idle_buddies"));
  gtk_switch_set_state (chatty->prefs_switch_convert_smileys,
                        purple_prefs_get_bool (CHATTY_PREFS_ROOT "/conversations/convert_emoticons"));

  g_signal_connect (chatty->prefs_switch_typing_notification,
                    "state-set",
                    G_CALLBACK(cb_switch_on_off_state_changed),
                    (gpointer)CHATTY_PREF_TYPING_NOTIFICATION);
  g_signal_connect (chatty->prefs_switch_show_offline,
                    "state-set",
                    G_CALLBACK(cb_switch_on_off_state_changed),
                    (gpointer)CHATTY_PREF_SHOW_OFFLINE);
  g_signal_connect (chatty->prefs_switch_indicate_offline,
                    "state-set",
                    G_CALLBACK(cb_switch_on_off_state_changed),
                    (gpointer)CHATTY_PREF_INDICATE_OFFLINE);
  g_signal_connect (chatty->prefs_switch_indicate_idle,
                    "state-set", G_CALLBACK(cb_switch_on_off_state_changed),
                    (gpointer)CHATTY_PREF_INDICATE_IDLE);
  g_signal_connect (chatty->prefs_switch_convert_smileys,
                    "state-set",
                    G_CALLBACK(cb_switch_on_off_state_changed),
                    (gpointer)CHATTY_PREF_CONVERT_SMILEY);
}


void
chatty_window_activate (GtkApplication *app,
                        gpointer        user_data)
{
  GtkBuilder         *builder;
  GtkWindow          *window;
  GSimpleActionGroup *simple_action_group;
  GtkCssProvider     *cssProvider = gtk_css_provider_new();

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

  gtk_window_set_default_size (GTK_WINDOW (window), 400, 640);

  chatty_popover_actions_init (window);

  chatty->main_window = window;

  gtk_css_provider_load_from_resource (cssProvider,
                                       "/sm/puri/chatty/css/style.css");
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default(),
                                             GTK_STYLE_PROVIDER (cssProvider),
                                             GTK_STYLE_PROVIDER_PRIORITY_USER);

  chatty->prefs_switch_send_receipts = GTK_SWITCH (gtk_builder_get_object (builder, "pref_send_receipts"));
  chatty->prefs_switch_carbon_copy = GTK_SWITCH (gtk_builder_get_object (builder, "pref_carbon_copy"));
  chatty->prefs_switch_typing_notification = GTK_SWITCH (gtk_builder_get_object (builder, "pref_typing_notification"));
  chatty->prefs_switch_show_offline = GTK_SWITCH (gtk_builder_get_object (builder, "pref_show_offline"));
  chatty->prefs_switch_indicate_offline = GTK_SWITCH (gtk_builder_get_object (builder, "pref_indicate_offline"));
  chatty->prefs_switch_indicate_idle = GTK_SWITCH (gtk_builder_get_object (builder, "pref_indicate_idle"));
  chatty->prefs_switch_convert_smileys = GTK_SWITCH (gtk_builder_get_object (builder, "pref_convert_smileys"));
  chatty->prefs_switch_return_sends = GTK_SWITCH (gtk_builder_get_object (builder, "pref_return_sends"));

  chatty->header_view_message_list = GTK_HEADER_BAR (gtk_builder_get_object (builder, "header_view_message_list"));
  chatty->header_icon = GTK_WIDGET (gtk_builder_get_object (builder, "header_icon"));
  chatty->header_spinner = GTK_WIDGET (gtk_builder_get_object (builder, "header_spinner"));
  chatty->panes_stack = GTK_STACK (gtk_builder_get_object (builder, "panes_stack"));
  chatty->pane_view_message_list = GTK_WIDGET (gtk_builder_get_object (builder, "pane_view_message_list"));

  chatty->pane_view_new_account = GTK_BOX (gtk_builder_get_object (builder, "pane_view_new_account"));
  chatty->pane_view_new_chat = GTK_BOX (gtk_builder_get_object (builder, "pane_view_new_chat"));
  chatty->pane_view_select_account = GTK_BOX (gtk_builder_get_object (builder, "pane_view_select_account"));
  chatty->pane_view_new_contact = GTK_BOX (gtk_builder_get_object (builder, "pane_view_new_contact"));
  chatty->pane_view_chat_list = GTK_BOX (gtk_builder_get_object (builder, "pane_view_chat_list"));

  chatty->account_list_manage = GTK_LIST_BOX (gtk_builder_get_object (builder, "manage_account_listbox"));
  chatty->account_list_select = GTK_LIST_BOX (gtk_builder_get_object (builder, "select_account_listbox"));
  chatty->list_privacy_prefs = GTK_LIST_BOX (gtk_builder_get_object (builder, "privacy_prefs_listbox"));
  chatty->list_xmpp_prefs = GTK_LIST_BOX (gtk_builder_get_object (builder, "xmpp_prefs_listbox"));
  chatty->list_general_prefs = GTK_LIST_BOX (gtk_builder_get_object (builder, "general_prefs_listbox"));

  gtk_list_box_set_header_func (chatty->account_list_manage, hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (chatty->account_list_select, hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (chatty->list_privacy_prefs, hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (chatty->list_xmpp_prefs, hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (chatty->list_general_prefs, hdy_list_box_separator_header, NULL, NULL);

  g_object_unref (builder);
  gtk_widget_show_all (GTK_WIDGET (window));
  chatty_window_init_data ();
}
