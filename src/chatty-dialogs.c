/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include "chatty-window.h"
#include "chatty-manager.h"
#include "chatty-account.h"
#include "chatty-buddy-list.h"
#include "chatty-conversation.h"
#include "chatty-purple-init.h"
#include "chatty-config.h"
#include "chatty-dialogs.h"
#include "chatty-lurch.h"
#include "chatty-utils.h"
#include "chatty-icons.h"
#include "chatty-dbus.h"
#include "chatty-folks.h"
#include "version.h"

#include <libebook-contacts/libebook-contacts.h>

static void chatty_dialogs_reset_new_contact_dialog (void);
static void chatty_dialogs_reset_invite_contact_dialog (void);
static char *chatty_dialogs_show_dialog_load_avatar (void);
static void chatty_dialogs_update_user_avatar (PurpleBuddy *buddy, const char *color);

static chatty_dialog_data_t chatty_dialog_data;

chatty_dialog_data_t *chatty_get_dialog_data (void)
{
  return &chatty_dialog_data;
}

static void
cb_switch_prefs_state_changed (GtkSwitch  *widget,
                               GParamSpec *pspec,
                               gpointer    data)
{
  gboolean        state;

  state = gtk_switch_get_active (widget);

  switch (GPOINTER_TO_INT(data)) {
    case CHATTY_PREF_MUC_NOTIFICATIONS:
      chatty_conv_set_muc_prefs (CHATTY_PREF_MUC_NOTIFICATIONS, state);
      break;
    case CHATTY_PREF_MUC_STATUS_MSG:
      chatty_conv_set_muc_prefs (CHATTY_PREF_MUC_STATUS_MSG, state);
      break;
    case CHATTY_PREF_MUC_PERSISTANT:
      chatty_conv_set_muc_prefs (CHATTY_PREF_MUC_PERSISTANT, state);
      break;
    default:
      break;
  }
}


static void
cb_switch_omemo_state_changed (GtkSwitch  *widget,
                               GParamSpec *pspec,
                               gpointer    user_data)
{
  PurpleConversation *conv;

  conv = (PurpleConversation *) user_data;

  gtk_switch_get_active (widget) ? chatty_lurch_enable (conv) : chatty_lurch_disable (conv);

  chatty_lurch_get_status (conv);
}


static void
cb_switch_notify_state_changed (GtkSwitch  *widget,                                
                                GParamSpec *pspec,
                                gpointer    user_data)
{
  PurpleBuddy        *buddy;
  PurpleConversation *conv;

  conv = (PurpleConversation *) user_data;

  buddy = purple_find_buddy (conv->account, conv->name);

  purple_blist_node_set_bool (PURPLE_BLIST_NODE(buddy), 
                              "chatty-notifications", 
                              gtk_switch_get_active (widget));
}


static gboolean
cb_dialog_delete (GtkWidget *widget,
                  GdkEvent  *event,
                  gpointer   user_data)
{
  gtk_widget_hide_on_delete (widget);

  return TRUE;
}


static void
cb_button_new_chat_back_clicked (GtkButton *sender,
                                 gpointer   data)
{
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  gtk_stack_set_visible_child_name (chatty_dialog->stack_panes_new_chat,
                                    "view-new-chat");
}


static void
cb_button_muc_info_back_clicked (GtkButton *sender,
                                 gpointer   data)
{
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  gtk_stack_set_visible_child_name (chatty_dialog->stack_panes_muc_info,
                                    "view-muc-info");
}


static void
chatty_entry_contact_name_check (GtkEntry  *entry,
                                 GtkWidget *button)
{
  PurpleBuddy *buddy;
  const char  *name;

  chatty_data_t *chatty = chatty_get_data ();

  name = gtk_entry_get_text (entry);

  if ((*name != '\0') && chatty->selected_account) {
    buddy = purple_find_buddy (chatty->selected_account, name);
  }

  if ((*name != '\0') && !buddy) {
    gtk_widget_set_sensitive (button, TRUE);
  } else {
    gtk_widget_set_sensitive (button, FALSE);
  }
}


static void
cb_contact_name_insert_text (GtkEntry    *entry,
                             const gchar *text,
                             gint         length,
                             gint        *position,
                             gpointer     data)
{
  chatty_entry_contact_name_check (entry, GTK_WIDGET(data));
}


static void
cb_contact_name_delete_text (GtkEntry    *entry,
                             gint         start_pos,
                             gint         end_pos,
                             gpointer     data)
{
  chatty_entry_contact_name_check (entry, GTK_WIDGET(data));
}


static void
cb_invite_name_insert_text (GtkEntry    *entry,
                            const gchar *text,
                            gint         length,
                            gint        *position,
                            gpointer     data)
{
  gtk_widget_set_sensitive (GTK_WIDGET(data), *position ? TRUE : FALSE);
}


static void
cb_invite_name_delete_text (GtkEntry    *entry,
                            gint         start_pos,
                            gint         end_pos,
                            gpointer     data)
{
  gtk_widget_set_sensitive (GTK_WIDGET(data), start_pos ? TRUE : FALSE);
}


static void
cb_button_show_add_contact_clicked (GtkButton *sender,
                                    gpointer   data)
{
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  chatty_dialogs_reset_new_contact_dialog ();

  gtk_stack_set_visible_child_name (chatty_dialog->stack_panes_new_chat,
                                    "view-new-contact");
}


static void
cb_button_add_gnome_contact_clicked (GtkButton *sender,
                                     gpointer   data)
{
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  chatty_dbus_gc_write_contact ("", "");
   
  gtk_stack_set_visible_child_name (chatty_dialog->stack_panes_new_chat,
                                    "view-new-chat");
}

static void
cb_button_add_contact_clicked (GtkButton *sender,
                               gpointer   data)
{
  char              *who;
  const char        *alias;
  g_autoptr(GError)  err = NULL;

  chatty_data_t        *chatty = chatty_get_data ();

  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  who = g_strdup (gtk_entry_get_text (GTK_ENTRY(chatty_dialog->entry_contact_name)));
  alias = gtk_entry_get_text (GTK_ENTRY(chatty_dialog->entry_contact_nick));

  chatty_blist_add_buddy (chatty->selected_account, who, alias);

  chatty_conv_im_with_buddy (chatty->selected_account, g_strdup (who));

  gtk_widget_hide (GTK_WIDGET(chatty->dialog_new_chat));

  g_free (who);

  gtk_entry_set_text (GTK_ENTRY(chatty_dialog->entry_contact_name), "");
  gtk_entry_set_text (GTK_ENTRY(chatty_dialog->entry_contact_nick), "");

  gtk_stack_set_visible_child_name (chatty_dialog->stack_panes_new_chat,
                                    "view-new-chat");
}


static void
cb_button_show_invite_contact_clicked (GtkButton *sender,
                                       gpointer   data)
{
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  chatty_dialogs_reset_invite_contact_dialog ();

  gtk_widget_grab_focus (GTK_WIDGET(chatty_dialog->entry_invite_name));

  gtk_stack_set_visible_child_name (chatty_dialog->stack_panes_muc_info,
                                    "view-invite-contact");
}


static void
cb_button_invite_contact_clicked (GtkButton *sender,
                                  gpointer   data)
{
  const char *name;
  const char *invite_msg;

  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  name = gtk_entry_get_text (GTK_ENTRY(chatty_dialog->entry_invite_name));
  invite_msg = gtk_entry_get_text (GTK_ENTRY(chatty_dialog->entry_invite_msg));

  if (name != NULL) {
    chatty_conv_invite_muc_user (name, invite_msg);
  }

  gtk_stack_set_visible_child_name (chatty_dialog->stack_panes_muc_info,
                                    "view-muc-info");
}


static void
cb_button_edit_topic_clicked (GtkToggleButton *sender,
                              gpointer         data)
{
  GtkStyleContext *sc;
  GtkTextIter      start, end;
  gboolean         edit_mode;

  chatty_data_t        *chatty = chatty_get_data ();
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  sc = gtk_widget_get_style_context (GTK_WIDGET(chatty_dialog->box_topic_frame));

  gtk_text_buffer_get_bounds (chatty->muc.msg_buffer_topic,
                              &start,
                              &end);

  if (gtk_toggle_button_get_active (sender)) {
    edit_mode = TRUE;

    chatty_dialog->current_topic = gtk_text_buffer_get_text (chatty->muc.msg_buffer_topic,
                                                             &start, &end,
                                                             FALSE);

    gtk_widget_grab_focus (GTK_WIDGET(chatty_dialog->textview_muc_topic));

    gtk_style_context_remove_class (sc, "topic_no_edit");
    gtk_style_context_add_class (sc, "topic_edit");
  } else {
    edit_mode = FALSE;

    if (g_strcmp0 (chatty_dialog->current_topic, chatty_dialog->new_topic) != 0) {
      chatty_conv_set_muc_topic (chatty_dialog->new_topic);
    }

    gtk_style_context_remove_class (sc, "topic_edit");
    gtk_style_context_add_class (sc, "topic_no_edit");
  }

  gtk_text_view_set_editable (GTK_TEXT_VIEW(chatty_dialog->textview_muc_topic), edit_mode);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW(chatty_dialog->textview_muc_topic), edit_mode);
  gtk_widget_set_can_focus (GTK_WIDGET(chatty_dialog->textview_muc_topic), edit_mode);
}


static void
cb_button_user_avatar_clicked (GtkButton *sender,
                               gpointer   data)
{
  PurpleContact *contact;
  char          *file_name = NULL;
  
  file_name = chatty_dialogs_show_dialog_load_avatar ();

  if (file_name) {
    contact = purple_buddy_get_contact ((PurpleBuddy*)data);

    purple_buddy_icons_node_set_custom_icon_from_file ((PurpleBlistNode*)contact, file_name);

    chatty_dialogs_update_user_avatar ((PurpleBuddy*)data, CHATTY_COLOR_BLUE);
  }

  g_free (file_name);
}


static gboolean
cb_textview_key_released (GtkWidget   *widget,
                          GdkEventKey *key_event,
                          gpointer     data)
{
  GtkTextIter start, end;

  chatty_data_t        *chatty = chatty_get_data ();
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  gtk_text_buffer_get_bounds (chatty->muc.msg_buffer_topic,
                              &start,
                              &end);

  chatty_dialog->new_topic = gtk_text_buffer_get_text (chatty->muc.msg_buffer_topic,
                                                       &start, &end,
                                                       FALSE);

  return TRUE;
}


static void
chatty_dialogs_reset_new_contact_dialog (void)
{
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  gtk_entry_set_text (GTK_ENTRY(chatty_dialog->entry_contact_name), "");
  gtk_entry_set_text (GTK_ENTRY(chatty_dialog->entry_contact_nick), "");

  chatty_account_populate_account_list (chatty_dialog->list_select_account,
                                        LIST_SELECT_CHAT_ACCOUNT);
}


static void
chatty_dialogs_reset_invite_contact_dialog (void)
{
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  chatty_account_populate_account_list (chatty_dialog->list_select_account,
                                        LIST_SELECT_CHAT_ACCOUNT);
}


GtkWidget *
chatty_dialogs_create_dialog_new_chat (void)
{
  GtkBuilder *builder;
  GtkWidget  *dialog;
  GtkWidget  *button_back;
  GtkWidget  *button_show_add_contact;
  GtkWindow  *window;

  chatty_data_t        *chatty = chatty_get_data ();
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  builder = gtk_builder_new_from_resource ("/sm/puri/chatty/ui/chatty-dialog-new-chat.ui");

  dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));

  chatty->pane_view_new_chat = GTK_BOX (gtk_builder_get_object (builder, "pane_view_new_chat"));
  chatty->search_entry_contacts = GTK_ENTRY (gtk_builder_get_object (builder, "search_entry_contacts"));
  chatty_dialog->grid_edit_contact = GTK_WIDGET (gtk_builder_get_object (builder, "grid_edit_contact"));
  chatty_dialog->button_add_gnome_contact = GTK_WIDGET (gtk_builder_get_object (builder, "button_add_gnome_contact"));
  chatty_dialog->stack_panes_new_chat = GTK_STACK (gtk_builder_get_object (builder, "stack_panes_new_chat"));
  chatty_dialog->list_select_account = GTK_LIST_BOX (gtk_builder_get_object (builder, "list_select_chat_account"));
  chatty_dialog->entry_contact_name = GTK_ENTRY (gtk_builder_get_object (builder, "entry_contact_name"));
  chatty_dialog->entry_contact_nick = GTK_ENTRY (gtk_builder_get_object (builder, "entry_contact_alias"));
  chatty_dialog->button_add_contact = GTK_WIDGET (gtk_builder_get_object (builder, "button_add_contact"));
  button_back = GTK_WIDGET (gtk_builder_get_object (builder, "button_back"));
  button_show_add_contact = GTK_WIDGET (gtk_builder_get_object (builder, "button_show_add_contact"));

  g_signal_connect (G_OBJECT(dialog),
                    "delete-event",
                    G_CALLBACK(cb_dialog_delete),
                    NULL);

  g_signal_connect (G_OBJECT(button_back),
                    "clicked",
                    G_CALLBACK(cb_button_new_chat_back_clicked),
                    NULL);

  g_signal_connect (G_OBJECT(button_show_add_contact),
                    "clicked",
                    G_CALLBACK(cb_button_show_add_contact_clicked),
                    NULL);

  g_signal_connect (G_OBJECT(chatty_dialog->button_add_contact),
                    "clicked",
                    G_CALLBACK(cb_button_add_contact_clicked),
                    NULL);

  g_signal_connect (G_OBJECT(chatty_dialog->button_add_gnome_contact),
                    "clicked",
                    G_CALLBACK(cb_button_add_gnome_contact_clicked),
                    NULL);

  gtk_list_box_set_header_func (chatty_dialog->list_select_account,
                                hdy_list_box_separator_header,
                                NULL, NULL);

  g_signal_connect_after (G_OBJECT(chatty_dialog->entry_contact_name),
                          "insert_text",
                          G_CALLBACK(cb_contact_name_insert_text),
                          (gpointer)chatty_dialog->button_add_contact);

  g_signal_connect_after (G_OBJECT(chatty_dialog->entry_contact_name),
                          "delete_text",
                          G_CALLBACK(cb_contact_name_delete_text),
                          (gpointer)chatty_dialog->button_add_contact);

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
  gtk_window_set_transient_for (GTK_WINDOW(dialog), window);

  g_object_unref (builder);

  return dialog;
}


GtkWidget *
chatty_dialogs_create_dialog_muc_info (void)
{
  GtkBuilder *builder;
  GtkWidget  *dialog;
  GtkWidget  *button_back;
  GtkWidget  *button_invite_contact;
  GtkWidget  *button_show_invite_contact;
  GtkWindow  *window;
  GtkListBox *list_muc_settings;

  chatty_data_t        *chatty = chatty_get_data ();
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  builder = gtk_builder_new_from_resource ("/sm/puri/chatty/ui/chatty-dialog-muc-info.ui");

  dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));

  chatty->pane_view_muc_info = GTK_BOX (gtk_builder_get_object (builder, "pane_view_muc_info"));
  chatty->muc.label_chat_id = GTK_WIDGET (gtk_builder_get_object (builder, "muc.label_chat_id"));
  chatty->muc.label_num_user = GTK_WIDGET (gtk_builder_get_object (builder, "muc.label_num_user"));
  chatty->muc.label_topic = GTK_WIDGET (gtk_builder_get_object (builder, "muc.label_topic"));
  chatty->muc.label_title = GTK_WIDGET (gtk_builder_get_object (builder, "muc.label_title"));
  chatty->muc.button_edit_topic = GTK_WIDGET (gtk_builder_get_object (builder, "muc.button_edit_topic"));
  chatty->muc.box_topic_editor = GTK_WIDGET (gtk_builder_get_object (builder, "muc.box_topic_editor"));
  chatty->muc.switch_prefs_notifications = GTK_SWITCH (gtk_builder_get_object (builder, "pref_muc_notifications"));
  chatty->muc.switch_prefs_status_msg = GTK_SWITCH (gtk_builder_get_object (builder, "pref_muc_status_msg"));
  chatty->muc.switch_prefs_persistant = GTK_SWITCH (gtk_builder_get_object (builder, "pref_muc_persistant"));
  chatty_dialog->box_topic_frame = GTK_WIDGET (gtk_builder_get_object (builder, "box_topic_frame"));
  chatty_dialog->textview_muc_topic = GTK_WIDGET (gtk_builder_get_object (builder, "textview_muc_topic"));
  chatty_dialog->stack_panes_muc_info = GTK_STACK (gtk_builder_get_object (builder, "stack_panes_muc_info"));
  chatty_dialog->entry_invite_name = GTK_ENTRY (gtk_builder_get_object (builder, "entry_invite_name"));
  chatty_dialog->entry_invite_msg = GTK_ENTRY (gtk_builder_get_object (builder, "entry_invite_msg"));
  button_back = GTK_WIDGET (gtk_builder_get_object (builder, "button_back"));
  button_show_invite_contact = GTK_WIDGET (gtk_builder_get_object (builder, "button_show_invite_contact"));
  button_invite_contact = GTK_WIDGET (gtk_builder_get_object (builder, "button_invite_contact"));

  list_muc_settings = GTK_LIST_BOX (gtk_builder_get_object (builder, "list_muc_settings"));
  gtk_list_box_set_header_func (list_muc_settings, hdy_list_box_separator_header, NULL, NULL);

  chatty->muc.msg_buffer_topic = gtk_text_buffer_new (NULL);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW(chatty_dialog->textview_muc_topic),
                            chatty->muc.msg_buffer_topic);

  g_signal_connect_after (G_OBJECT(chatty_dialog->textview_muc_topic),
                          "key-release-event",
                          G_CALLBACK(cb_textview_key_released),
                          NULL);

  g_signal_connect (G_OBJECT(chatty->muc.button_edit_topic),
                    "clicked",
                    G_CALLBACK (cb_button_edit_topic_clicked),
                    NULL);

  g_signal_connect (chatty->muc.switch_prefs_notifications,
                    "notify::active",
                    G_CALLBACK(cb_switch_prefs_state_changed),
                    (gpointer)CHATTY_PREF_MUC_NOTIFICATIONS);

  g_signal_connect (chatty->muc.switch_prefs_status_msg,
                    "notify::active",
                    G_CALLBACK(cb_switch_prefs_state_changed),
                    (gpointer)CHATTY_PREF_MUC_STATUS_MSG);

  g_signal_connect (chatty->muc.switch_prefs_persistant,
                    "notify::active",
                    G_CALLBACK(cb_switch_prefs_state_changed),
                    (gpointer)CHATTY_PREF_MUC_PERSISTANT);

  g_signal_connect (G_OBJECT(dialog),
                    "delete-event",
                    G_CALLBACK(cb_dialog_delete),
                    NULL);

  g_signal_connect (G_OBJECT(button_back),
                    "clicked",
                    G_CALLBACK (cb_button_muc_info_back_clicked),
                    NULL);

  g_signal_connect (G_OBJECT(button_show_invite_contact),
                    "clicked",
                    G_CALLBACK (cb_button_show_invite_contact_clicked),
                    NULL);

  g_signal_connect (G_OBJECT(button_invite_contact),
                    "clicked",
                    G_CALLBACK (cb_button_invite_contact_clicked),
                    NULL);

  gtk_list_box_set_header_func (chatty_dialog->list_select_account,
                                hdy_list_box_separator_header,
                                NULL, NULL);

  g_signal_connect_after (G_OBJECT(chatty_dialog->entry_invite_name),
                          "insert_text",
                          G_CALLBACK(cb_invite_name_insert_text),
                          (gpointer)button_invite_contact);

  g_signal_connect_after (G_OBJECT(chatty_dialog->entry_invite_name),
                          "delete_text",
                          G_CALLBACK(cb_invite_name_delete_text),
                          (gpointer)button_invite_contact);

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
  gtk_window_set_transient_for (GTK_WINDOW(dialog), window);

  g_object_unref (builder);

  return dialog;
}


void
chatty_dialogs_show_dialog_join_muc (void)
{
  GtkBuilder *builder;
  GtkWidget  *dialog;
  GtkWidget  *button_join_chat;
  GtkWindow  *window;
  GtkListBox *list_select_muc_account;
  GtkEntry   *entry_group_chat_id;
  GtkEntry   *entry_group_chat_room_alias;
  GtkEntry   *entry_group_chat_user_alias;
  GtkEntry   *entry_group_chat_pw;
  int         response;

  chatty_data_t *chatty = chatty_get_data ();

  builder = gtk_builder_new_from_resource ("/sm/puri/chatty/ui/chatty-dialog-join-muc.ui");

  dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));

  list_select_muc_account = GTK_LIST_BOX (gtk_builder_get_object (builder, "list_select_muc_account"));
  button_join_chat = GTK_WIDGET (gtk_builder_get_object (builder, "button_join_chat"));
  entry_group_chat_id = GTK_ENTRY (gtk_builder_get_object (builder, "entry_group_chat_id"));
  entry_group_chat_room_alias = GTK_ENTRY (gtk_builder_get_object (builder, "entry_group_chat_room_alias"));
  entry_group_chat_user_alias = GTK_ENTRY (gtk_builder_get_object (builder, "entry_group_chat_user_alias"));
  entry_group_chat_pw = GTK_ENTRY (gtk_builder_get_object (builder, "entry_group_chat_pw"));

  gtk_list_box_set_header_func (list_select_muc_account,
                                hdy_list_box_separator_header,
                                NULL, NULL);

  g_signal_connect (G_OBJECT(entry_group_chat_id),
                    "insert_text",
                    G_CALLBACK(cb_contact_name_insert_text),
                    (gpointer)button_join_chat);

  chatty_account_populate_account_list (list_select_muc_account,
                                        LIST_SELECT_MUC_ACCOUNT);

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
  gtk_window_set_transient_for (GTK_WINDOW(dialog), window);

  response = gtk_dialog_run (GTK_DIALOG(dialog));

  if (response == GTK_RESPONSE_OK) {
    chatty_blist_join_group_chat (chatty->selected_account,
                                  gtk_entry_get_text (entry_group_chat_id),
                                  gtk_entry_get_text (entry_group_chat_room_alias),
                                  gtk_entry_get_text (entry_group_chat_user_alias),
                                  gtk_entry_get_text (entry_group_chat_pw));
  }

  gtk_widget_destroy (dialog);
  g_object_unref (builder);
}


static void 
chatty_dialogs_update_user_avatar (PurpleBuddy *buddy,
                                   const char  *color)
{
  PurpleContact *contact;
  GdkPixbuf     *icon;
  GtkWidget     *avatar;
  const char    *alias;
  const char    *buddy_alias;
  const char    *contact_alias;

  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  alias = purple_buddy_get_alias (buddy);

  icon = chatty_icon_get_buddy_icon (PURPLE_BLIST_NODE(buddy),
                                     alias,
                                     CHATTY_ICON_SIZE_LARGE,
                                     color,
                                     FALSE);
  
  if (icon != NULL) {
    avatar = gtk_image_new ();
    gtk_image_set_from_pixbuf (GTK_IMAGE(avatar), icon);
    gtk_button_set_image (GTK_BUTTON(chatty_dialog->button_user_avatar), GTK_WIDGET(avatar));
  }

  g_object_unref (icon);

  contact = purple_buddy_get_contact (buddy);
  buddy_alias = purple_buddy_get_alias (buddy);
  contact_alias = purple_contact_get_alias (contact);

  icon = chatty_icon_get_buddy_icon (PURPLE_BLIST_NODE(buddy),
                                     alias,
                                     CHATTY_ICON_SIZE_SMALL,
                                     color,
                                     FALSE);

  chatty_window_update_sub_header_titlebar (icon, contact_alias ? contact_alias : buddy_alias);

  g_object_unref (icon);
}


void
chatty_dialogs_show_dialog_user_info (ChattyConversation *chatty_conv)
{
  ChattyManager  *manager;
  PurpleBuddy    *buddy;
  PurpleAccount  *account;
  PurplePresence *presence;
  PurpleStatus   *status;
  GtkBuilder     *builder;
  GtkWidget      *dialog;
  GtkWidget      *label_alias;
  GtkWidget      *label_jid;
  GtkWidget      *label_encryption;
  GtkWidget      *label_user_id;
  GtkWidget      *label_user_status;
  GtkWidget      *label_status_msg;
  GtkWindow      *window;
  GtkSwitch      *switch_notify;
  GtkListBox     *listbox_prefs;
  const char     *protocol_id;
  const char     *alias;

  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  manager = chatty_manager_get_default ();
  builder = gtk_builder_new_from_resource ("/sm/puri/chatty/ui/chatty-dialog-user-info.ui");

  chatty_dialog->button_user_avatar = GTK_WIDGET (gtk_builder_get_object (builder, "button-user-avatar"));
  label_alias = GTK_WIDGET (gtk_builder_get_object (builder, "label_alias"));
  label_user_id = GTK_WIDGET (gtk_builder_get_object (builder, "label_user_id"));
  label_jid = GTK_WIDGET (gtk_builder_get_object (builder, "label_jid"));
  switch_notify = GTK_SWITCH (gtk_builder_get_object (builder, "switch_notify"));
  listbox_prefs = GTK_LIST_BOX (gtk_builder_get_object (builder, "listbox_prefs"));

  account = purple_conversation_get_account (chatty_conv->conv);
  protocol_id = purple_account_get_protocol_id (account);

  if (chatty_manager_lurch_plugin_is_loaded (manager) && (!g_strcmp0 (protocol_id, "prpl-jabber"))) {
    label_encryption = GTK_WIDGET (gtk_builder_get_object (builder, "label_encryption"));
    chatty_dialog->omemo.switch_on_off = GTK_SWITCH (gtk_builder_get_object (builder, "switch_omemo"));
    chatty_dialog->omemo.label_status_msg = GTK_WIDGET (gtk_builder_get_object (builder, "label_encryption_msg"));
    chatty_dialog->omemo.listbox_fp_contact = GTK_LIST_BOX (gtk_builder_get_object (builder, "listbox_fp"));

    gtk_widget_show (GTK_WIDGET(listbox_prefs));
    gtk_widget_show (GTK_WIDGET(chatty_dialog->omemo.listbox_fp_contact));
    gtk_widget_show (GTK_WIDGET(label_encryption));
    gtk_widget_show (GTK_WIDGET(chatty_dialog->omemo.label_status_msg));

    gtk_list_box_set_header_func (chatty_dialog->omemo.listbox_fp_contact,
                                  hdy_list_box_separator_header,
                                  NULL, NULL);

    gtk_label_set_text (GTK_LABEL(label_user_id), "XMPP ID");

    chatty_lurch_get_status (chatty_conv->conv);
    chatty_lurch_get_fp_list_contact (chatty_conv->conv);

    gtk_switch_set_state (chatty_dialog->omemo.switch_on_off, chatty_conv->omemo.enabled);

    g_signal_connect (chatty_dialog->omemo.switch_on_off,
                      "notify::active",
                      G_CALLBACK(cb_switch_omemo_state_changed),
                      (gpointer)chatty_conv->conv);
  }

  if (!g_strcmp0 (protocol_id, "prpl-mm-sms")) {
    gtk_label_set_text (GTK_LABEL(label_user_id), _("Phone Number:"));
  }

  gtk_list_box_set_header_func (listbox_prefs,
                                hdy_list_box_separator_header,
                                NULL, NULL);

  buddy = purple_find_buddy (chatty_conv->conv->account, chatty_conv->conv->name);
  alias = purple_buddy_get_alias (buddy);

  if (chatty_blist_protocol_is_sms (account)) {
    chatty_dialogs_update_user_avatar (buddy, CHATTY_COLOR_GREEN);
  } else {
    chatty_dialogs_update_user_avatar (buddy, CHATTY_COLOR_BLUE);

    g_signal_connect (G_OBJECT(chatty_dialog->button_user_avatar),
                      "clicked",
                      G_CALLBACK (cb_button_user_avatar_clicked),
                      buddy);
  } 
  
  gtk_switch_set_state (switch_notify,
                        purple_blist_node_get_bool (PURPLE_BLIST_NODE(buddy),
                        "chatty-notifications"));

  g_signal_connect (switch_notify,
                    "notify::active",
                    G_CALLBACK(cb_switch_notify_state_changed),
                    (gpointer)chatty_conv->conv);

  gtk_label_set_text (GTK_LABEL(label_alias), chatty_utils_jabber_id_strip (alias));
  gtk_label_set_text (GTK_LABEL(label_jid), chatty_conv->conv->name);

  if (!g_strcmp0 (protocol_id, "prpl-jabber")) {
    label_user_status = GTK_WIDGET (gtk_builder_get_object (builder, "label_user_status"));
    label_status_msg = GTK_WIDGET (gtk_builder_get_object (builder, "label_status_msg"));
    
    gtk_widget_show (GTK_WIDGET(label_user_status));
    gtk_widget_show (GTK_WIDGET(label_status_msg));

    presence = purple_buddy_get_presence (buddy);
    status = purple_presence_get_active_status (presence);

    gtk_label_set_text (GTK_LABEL(label_status_msg), purple_status_get_name (status));
  }

  dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
  gtk_window_set_transient_for (GTK_WINDOW(dialog), window);

  gtk_dialog_run (GTK_DIALOG(dialog));

  gtk_widget_destroy (dialog);
  g_object_unref (builder);
}


void
chatty_dialogs_show_dialog_about_chatty (void)
{
  GtkWindow *window;

  static const gchar *authors[] = {
    "Adrien Plazas <kekun.plazas@laposte.net>",
    "Andrea Schäfer <mosibasu@me.com>",
    "Benedikt Wildenhain <benedikt.wildenhain@hs-bochum.de>",
    "Guido Günther <agx@sigxcpu.org>",
    "Julian Sparber <jsparber@gnome.org>",
    "Leland Carlye <leland.carlye@protonmail.com>",
    "Mohammed Sadiq https://www.sadiqpk.org/",
    "Richard Bayerle (OMEMO Plugin) https://github.com/gkdr/lurch",
    "Ruslan Marchenko <me@ruff.mobi>",
    "and more...",
    NULL
  };

  static const gchar *artists[] = {
    "Tobias Bernard <tbernard@gnome.org>",
    NULL
  };

  static const gchar *documenters[] = {
    "Heather Ellsworth <heather.ellsworth@puri.sm>",
    NULL
  };

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));

  gtk_show_about_dialog (GTK_WINDOW(window),
                         "logo-icon-name", CHATTY_APP_ID,
                         "program-name", _("Chats"),
                         "version", GIT_VERSION,
                         "comments", _("An SMS and XMPP messaging client"),
                         "website", "https://source.puri.sm/Librem5/chatty",
                         "copyright", "© 2018 Purism SPC",
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "authors", authors,
                         "artists", artists,
                         "documenters", documenters,
                         "translator-credits", _("translator-credits"),
                         NULL);
}


static char * 
chatty_dialogs_show_dialog_load_avatar (void) 
{
  GtkWindow            *window;
  GtkFileChooserNative *dialog;
  gchar                *file_name;
  int                   response;

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));

  dialog = gtk_file_chooser_native_new (_("Set Avatar"),
                                        window,
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("Open"),
                                        _("Cancel"));

  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog), getenv ("HOME"));

  // TODO: add preview widget when available in portrait mode

  response = gtk_native_dialog_run (GTK_NATIVE_DIALOG(dialog));

  if (response == GTK_RESPONSE_ACCEPT) {
    file_name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(dialog));
  } else {
    file_name = NULL;
  }

  g_object_unref (dialog);

  return file_name;
}
