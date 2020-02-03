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
#include "chatty-conversation.h"
#include "chatty-account.h"
#include "chatty-purple-init.h"
#include "chatty-icons.h"
#include "chatty-popover-actions.h"
#include "dialogs/chatty-dialogs.h"
#include "dialogs/chatty-settings-dialog.h"
#include "dialogs/chatty-new-chat-dialog.h"
#include "dialogs/chatty-new-muc-dialog.h"
#include "dialogs/chatty-user-info-dialog.h"
#include "dialogs/chatty-muc-info-dialog.h"

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


overlay_content_t OverlayContent[6] = {
  {.title      = N_("Choose a contact"),
   .text_1     = N_("Select an <b>SMS</b> or <b>Instant Message</b> contact with the <b>\"+\"</b> button in the titlebar."),
   .text_2     = NULL,
  },
  {.title      = N_("Choose a contact"),
   .text_1     = N_("Select an <b>Instant Message</b> contact with the \"+\" button in the titlebar."),
   .text_2     = NULL,
  },
  {.title      = N_("Choose a contact"),
   .text_1     = N_("Start a <b>SMS</b> chat with with the \"+\" button in the titlebar."),
   .text_2     = N_("For <b>Instant Messaging</b> add or activate an account in <i>\"preferences\"</i>."),
  },
  {.title      = N_("Start chatting"),
   .text_1     = N_("For <b>Instant Messaging</b> add or activate an account in <i>\"preferences\"</i>."),
   .text_2     = NULL,
  }
};


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

  if (fold != HDY_FOLD_FOLDED) {
    chatty_blist_chat_list_select_first ();
  }

  chatty_update_header ();
}


static gboolean
cb_window_delete (GtkWidget *widget,
                  GdkEvent  *event,
                  gpointer   user_data)
{
  gtk_widget_hide_on_delete (widget);

  return TRUE;
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
chatty_window_show_settings_dialog (void)
{
  GtkWindow *window;
  GtkWidget *dialog;

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
  dialog = chatty_settings_dialog_new (window);
  gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);
}


static void
chatty_window_show_new_muc_dialog (void)
{
  GtkWindow *window;
  GtkWidget *dialog;

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
  dialog = chatty_new_muc_dialog_new (window);
  gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);
}


static GtkWidget *
chatty_window_create_new_chat_dialog (void)
{
  GtkWindow *window;
  GtkWidget *dialog;

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
  dialog = chatty_new_chat_dialog_new (window);

  return dialog;
}


static void
chatty_window_show_user_info_dialog (ChattyConversation *chatty_conv)
{
  GtkWindow *window;
  GtkWidget *dialog;

  g_return_if_fail (chatty_conv != NULL);

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
  dialog = chatty_user_info_dialog_new (window, (gpointer)chatty_conv);
  gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);
}


static void
chatty_window_show_muc_info_dialog (ChattyConversation *chatty_conv)
{
  GtkWindow *window;
  GtkWidget *dialog;

  g_return_if_fail (chatty_conv != NULL);

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
  dialog = chatty_muc_info_dialog_new (window, (gpointer)chatty_conv);
  gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);
}


static void
chatty_window_show_chat_info (void)
{
  ChattyConversation *chatty_conv;

  chatty_data_t *chatty = chatty_get_data ();

  chatty_conv = chatty_conv_container_get_active_chatty_conv (GTK_NOTEBOOK(chatty->pane_view_message_list));

  if (purple_conversation_get_type (chatty_conv->conv) == PURPLE_CONV_TYPE_IM) {
    chatty_window_show_user_info_dialog (chatty_conv);

  } else if (purple_conversation_get_type (chatty_conv->conv) == PURPLE_CONV_TYPE_CHAT) {
    chatty_window_show_muc_info_dialog (chatty_conv);
  }
}


void
chatty_window_change_view (ChattyWindowState view)
{
  chatty_data_t *chatty = chatty_get_data ();

  switch (view) {
    case CHATTY_VIEW_SETTINGS:
      chatty_window_show_settings_dialog ();
      break;
    case CHATTY_VIEW_ABOUT_CHATTY:
      chatty_dialogs_show_dialog_about_chatty ();
      break;
    case CHATTY_VIEW_JOIN_CHAT:
      chatty_window_show_new_muc_dialog ();
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
chatty_window_overlay_show (gboolean show)
{
  gint   mode;
  guint8 accounts = 0;

  chatty_data_t *chatty = chatty_get_data ();

  if (show) {
    gtk_widget_show (GTK_WIDGET(chatty->box_overlay));
  } else {
    gtk_widget_hide (GTK_WIDGET(chatty->box_overlay));
    return;
  }

  if (chatty->sms_account_connected) {
    accounts |= CHATTY_ACCOUNTS_SMS;
  }

  if (chatty->im_account_connected ) {
    accounts |= CHATTY_ACCOUNTS_IM;
  }

  if (accounts == CHATTY_ACCOUNTS_IM_SMS) {
    mode = CHATTY_OVERLAY_EMPTY_CHAT;
  } else if (accounts == CHATTY_ACCOUNTS_SMS) {
    mode = CHATTY_OVERLAY_EMPTY_CHAT_NO_IM;
  } else if (accounts == CHATTY_ACCOUNTS_IM) {
    mode = CHATTY_OVERLAY_EMPTY_CHAT_NO_SMS;
  } else {
    mode = CHATTY_OVERLAY_EMPTY_CHAT_NO_SMS_IM;
  }

  gtk_image_set_from_icon_name (chatty->icon_overlay,
                                "sm.puri.Chatty-symbolic",
                                0);

  gtk_image_set_pixel_size (chatty->icon_overlay, 96);

  gtk_label_set_text (GTK_LABEL(chatty->label_overlay_1),
                      gettext (OverlayContent[mode].title));
  gtk_label_set_text (GTK_LABEL(chatty->label_overlay_2),
                      gettext (OverlayContent[mode].text_1));
  gtk_label_set_text (GTK_LABEL(chatty->label_overlay_3),
                      gettext (OverlayContent[mode].text_2));

  gtk_label_set_use_markup (GTK_LABEL(chatty->label_overlay_1), TRUE);
  gtk_label_set_use_markup (GTK_LABEL(chatty->label_overlay_2), TRUE);
  gtk_label_set_use_markup (GTK_LABEL(chatty->label_overlay_3), TRUE);
}


static void 
chatty_window_init_data (void)
{
  chatty_data_t *chatty = chatty_get_data ();

  // These dialogs need to be created before purple_blist_show()
  chatty->dialog_new_chat = chatty_window_create_new_chat_dialog ();

  libpurple_init ();

  hdy_leaflet_set_visible_child_name (chatty->content_box, "sidebar");

  hdy_search_bar_connect_entry (chatty->search_bar_chats,
                                chatty->search_entry_chats);

  gtk_widget_set_sensitive (GTK_WIDGET(chatty->button_header_sub_menu), FALSE);

  chatty_window_change_view (CHATTY_VIEW_CHAT_LIST);
}


void
chatty_window_activate (GtkApplication *app,
                        gpointer        user_data)
{
  GtkBuilder         *builder;
  GtkWindow          *window;
  GSimpleActionGroup *simple_action_group;

  chatty_data_t *chatty = chatty_get_data ();

  builder = gtk_builder_new_from_resource ("/sm/puri/chatty/ui/chatty-window.ui");

  window = GTK_WINDOW (gtk_builder_get_object (builder, "window"));
  g_object_set (window, "application", app, NULL);

  if (GPOINTER_TO_INT(user_data)) {
    g_signal_connect (G_OBJECT(window),
                      "delete-event",
                      G_CALLBACK(cb_window_delete),
                      NULL);
  }

  simple_action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   window_action_entries,
                                   G_N_ELEMENTS (window_action_entries),
                                   window);
  gtk_widget_insert_action_group (GTK_WIDGET (window),
                                  "win",
                                  G_ACTION_GROUP (simple_action_group));

  chatty_popover_actions_init (window);

  chatty->sub_header_label = GTK_WIDGET (gtk_builder_get_object (builder, "sub_header_label"));
  chatty->sub_header_icon = GTK_WIDGET (gtk_builder_get_object (builder, "sub_header_icon"));
  chatty->button_menu_add_contact = GTK_WIDGET (gtk_builder_get_object (builder, "button_menu_add_contact"));
  chatty->button_menu_add_gnome_contact = GTK_WIDGET (gtk_builder_get_object (builder, "button_menu_add_gnome_contact"));
  chatty->button_menu_new_group_chat = GTK_WIDGET (gtk_builder_get_object (builder, "button_menu_new_group_chat"));
  chatty->button_header_chat_info = GTK_WIDGET (gtk_builder_get_object (builder, "button_header_chat_info"));
  chatty->button_header_add_chat = GTK_WIDGET (gtk_builder_get_object (builder, "button_header_add_chat"));
  chatty->button_header_sub_menu = GTK_WIDGET (gtk_builder_get_object (builder, "button_header_sub_menu"));

  chatty->search_bar_chats = HDY_SEARCH_BAR (gtk_builder_get_object (builder, "search_bar_chats"));
  chatty->search_entry_chats = GTK_ENTRY (gtk_builder_get_object (builder, "search_entry_chats"));

  chatty->content_box = HDY_LEAFLET (gtk_builder_get_object (builder, "content_box"));
  chatty->header_box = HDY_LEAFLET (gtk_builder_get_object (builder, "header_box"));
  chatty->header_group = HDY_HEADER_GROUP (gtk_builder_get_object (builder, "header_group"));

  chatty->box_overlay = GTK_BOX (gtk_builder_get_object (builder, "welcome_overlay"));
  chatty->icon_overlay = GTK_IMAGE (gtk_builder_get_object (builder, "icon"));
  chatty->label_overlay_1 = GTK_WIDGET (gtk_builder_get_object (builder, "label_1"));
  chatty->label_overlay_2 = GTK_WIDGET (gtk_builder_get_object (builder, "label_2"));
  chatty->label_overlay_3 = GTK_WIDGET (gtk_builder_get_object (builder, "label_3"));

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

  chatty_window_init_data ();
}
