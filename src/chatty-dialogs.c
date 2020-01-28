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
#include "chatty-utils.h"
#include "version.h"


static chatty_dialog_data_t chatty_dialog_data;

chatty_dialog_data_t *chatty_get_dialog_data (void)
{
  return &chatty_dialog_data;
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
cb_button_muc_info_back_clicked (GtkButton *sender,
                                 gpointer   data)
{
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  gtk_stack_set_visible_child_name (chatty_dialog->stack_panes_muc_info,
                                    "view-muc-info");
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
cb_button_show_invite_contact_clicked (GtkButton *sender,
                                       gpointer   data)
{
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  //chatty_dialogs_reset_invite_contact_dialog ();

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


char * 
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
