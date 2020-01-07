/* -*- mode: c; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* chatty-settings-dialog.c
 *
 * Copyright 2020 Purism SPC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s):
 *   Andrea Schäfer <mosibasu@me.com>
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-settings-dialog"

#include <glib/gi18n.h>

#include "chatty-config.h"
#include "chatty-window.h"
#include "chatty-purple-init.h"
#include "chatty-lurch.h"
#include "chatty-dialogs.h"
#include "chatty-account.h"
#include "chatty-icons.h"
#include "chatty-settings.h"
#include "chatty-settings-dialog.h"

/**
 * @short_description: Chatty settings Dialog
 */

/* Several code has been copied from chatty-dialogs.c with modifications
 * which was written by Andrea Schäfer. */
struct _ChattySettingsDialog
{
  HdyDialog       parent_instance;

  GtkWidget      *add_button;
  GtkWidget      *save_button;

  GtkWidget      *main_stack;
  GtkWidget      *accounts_list_box;
  GtkWidget      *add_account_row;

  GtkWidget      *avatar_button;
  GtkWidget      *account_id_entry;
  GtkWidget      *account_protocol_label;
  GtkWidget      *status_label;
  GtkWidget      *password_entry;
  GtkWidget      *edit_password_button;

  GtkWidget      *protocol_list;
  GtkWidget      *protocol_title_label;
  GtkWidget      *xmpp_radio_button;
  GtkWidget      *matrix_row;
  GtkWidget      *matrix_radio_button;
  GtkWidget      *telegram_row;
  GtkWidget      *telegram_radio_button;
  GtkWidget      *new_account_settings_list;
  GtkWidget      *new_account_id_entry;
  GtkWidget      *server_url_entry;
  GtkWidget      *new_password_entry;

  GtkWidget      *fingerprint_list;
  GtkWidget      *fingerprint_device_list;

  GtkWidget      *send_receipts_switch;
  GtkWidget      *message_carbons_row;
  GtkWidget      *message_carbons_switch;
  GtkWidget      *typing_notification_switch;

  GtkWidget      *indicate_offline_switch;
  GtkWidget      *indicate_idle_switch;
  GtkWidget      *indicate_unknown_switch;

  GtkWidget      *convert_smileys_switch;
  GtkWidget      *return_sends_switch;

  ChattySettings *settings;
  PurpleAccount  *selected_account;
};

G_DEFINE_TYPE (ChattySettingsDialog, chatty_settings_dialog, HDY_TYPE_DIALOG)


static void
chatty_settings_dialog_update_status (ChattySettingsDialog *self,
                                      PurpleAccount        *account)
{
  const gchar *status;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  if (purple_account_is_connected (account))
    status = _("connected");
  else if (purple_account_is_connecting (account))
    status = _("connecting…");
  else
    status = _("disconnected");

  gtk_label_set_text (GTK_LABEL (self->status_label), status);
}

static void
chatty_account_list_clear (ChattySettingsDialog *self,
                           GtkListBox           *list)
{
  g_autoptr(GList) children = NULL;
  GList *iter;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));
  g_assert (GTK_IS_LIST_BOX (list));

  children = gtk_container_get_children (GTK_CONTAINER (list));

  for (iter = children; iter != NULL; iter = iter->next)
    if ((GtkWidget *)iter->data != self->add_account_row)
      gtk_container_remove (GTK_CONTAINER (list), GTK_WIDGET(iter->data));
}

static void
settings_update_account_details (ChattySettingsDialog *self)
{
  GdkPixbuf *pixbuf, *origin_pixbuf;
  PurpleStoredImage *image;
  PurpleAccount *account;
  GtkWidget *avatar;
  const char *account_name, *protocol_name;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  account = self->selected_account;
  account_name = purple_account_get_username (account);
  protocol_name = purple_account_get_protocol_name (account);

  image = purple_buddy_icons_find_account_icon (account);
  avatar = gtk_image_new ();

  gtk_entry_set_text (GTK_ENTRY (self->account_id_entry), account_name);
  gtk_label_set_text (GTK_LABEL (self->account_protocol_label), protocol_name);

  if (image != NULL)
    {
      pixbuf = chatty_icon_pixbuf_from_data (purple_imgstore_get_data (image),
                                             purple_imgstore_get_size (image));

    if (gdk_pixbuf_get_width (pixbuf) >= CHATTY_ICON_SIZE_LARGE ||
        gdk_pixbuf_get_height (pixbuf) >= CHATTY_ICON_SIZE_LARGE)
      {

        origin_pixbuf = g_object_ref (pixbuf);
        g_object_unref (pixbuf);

        pixbuf = gdk_pixbuf_scale_simple (origin_pixbuf,
                                          CHATTY_ICON_SIZE_LARGE,
                                          CHATTY_ICON_SIZE_LARGE,
                                          GDK_INTERP_BILINEAR);

        g_object_unref (origin_pixbuf);
      }

    gtk_image_set_from_pixbuf (GTK_IMAGE (avatar), chatty_icon_shape_pixbuf_circular (pixbuf));

    g_object_unref (pixbuf);
    purple_imgstore_unref (image);
  }
  else
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (avatar),
                                    "avatar-default-symbolic",
                                    GTK_ICON_SIZE_DIALOG);
  }

  gtk_button_set_image (GTK_BUTTON (self->avatar_button),
                        GTK_WIDGET (avatar));
}

static void
chatty_settings_add_clicked_cb (ChattySettingsDialog *self)
{
  PurpleAccount *account;
  const char *user_id, *password, *server_url;
  gboolean is_matrix, is_telegram;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  user_id  = gtk_entry_get_text (GTK_ENTRY (self->new_account_id_entry));
  password = gtk_entry_get_text (GTK_ENTRY (self->new_password_entry));
  server_url = gtk_entry_get_text (GTK_ENTRY (self->server_url_entry));

  is_matrix = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->matrix_radio_button));
  is_telegram = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->telegram_radio_button));

  if (is_matrix)
    {
      account = purple_account_new (user_id, "prpl-matrix");
      purple_account_set_string (account, "home_server", server_url);
    }
  else if (is_telegram)
    {
      account = purple_account_new (user_id, "prpl-telegram");
      purple_account_set_string (account, "password-two-factor", password);
    }
  else /* XMPP */
    {
      g_autofree char *name = NULL;
      const gchar *url_prefix = NULL;

      if (server_url && *server_url)
        url_prefix = "@";

      name = g_strconcat (user_id, url_prefix, server_url, NULL);
      account = purple_account_new (name, "prpl-jabber");
    }

  if (!is_telegram && password && *password)
    {
      purple_account_set_password (account, password);
      purple_account_set_remember_password (account, TRUE);
    }

  purple_account_set_enabled (account, CHATTY_UI, TRUE);
  purple_accounts_add (account);

  gtk_widget_hide (self->add_button);
  gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "main-settings");
}

static void
chatty_settings_save_clicked_cb (ChattySettingsDialog *self)
{
  GtkEntry *password_entry;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  purple_account_set_username (self->selected_account,
                               gtk_entry_get_text (GTK_ENTRY (self->account_id_entry)));

  password_entry = (GtkEntry *)self->password_entry;
  purple_account_set_password (self->selected_account,
                               gtk_entry_get_text (password_entry));

  purple_account_set_remember_password (self->selected_account, TRUE);
  purple_account_set_enabled (self->selected_account, CHATTY_UI, TRUE);

  gtk_widget_hide (self->save_button);
  gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "main-settings");
}

static void
settings_update_new_account_view (ChattySettingsDialog *self)
{
  PurplePlugin *protocol;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  gtk_entry_set_text (GTK_ENTRY (self->new_account_id_entry), "");
  gtk_entry_set_text (GTK_ENTRY (self->new_password_entry), "");
  gtk_entry_set_text (GTK_ENTRY (self->server_url_entry), "");

  self->selected_account = NULL;
  gtk_widget_grab_focus (self->new_account_id_entry);
  gtk_widget_show (self->add_button);

  protocol = purple_find_prpl ("prpl-matrix");
  gtk_widget_set_visible (self->matrix_row, protocol != NULL);

  protocol = purple_find_prpl ("prpl-telegram");
  gtk_widget_set_visible (self->telegram_row, protocol != NULL);

  if (gtk_widget_get_visible (self->matrix_row) ||
      gtk_widget_get_visible (self->telegram_row))
    {
      gtk_label_set_text (GTK_LABEL (self->protocol_title_label), _("Select Protocol"));
      gtk_widget_show (self->protocol_list);
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (self->protocol_title_label), _("Add XMPP account"));
      gtk_widget_hide (self->protocol_list);
    }

  gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "add-account-view");
}

static void
account_list_row_activated_cb (ChattySettingsDialog *self,
                               GtkListBoxRow        *row,
                               GtkListBox           *box)
{
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();
  chatty_purple_data_t *chatty_purple = chatty_get_purple_data ();

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (box));

  gtk_widget_set_sensitive (self->add_button, FALSE);
  gtk_widget_set_sensitive (self->save_button, FALSE);
  gtk_widget_set_sensitive (self->password_entry, FALSE);

  if (GTK_WIDGET (row) == self->add_account_row)
    {
      settings_update_new_account_view (self);
    }
  else
    {
      const gchar *protocol_id;

      gtk_widget_show (self->save_button);
      self->selected_account = g_object_get_data (G_OBJECT (row), "row-account");
      g_assert (self->selected_account != NULL);

      chatty_settings_dialog_update_status (self, self->selected_account);
      gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack),
                                        "edit-account-view");

      protocol_id = purple_account_get_protocol_id (self->selected_account);

      if (chatty_purple->plugin_lurch_loaded &&
          g_strcmp0 (protocol_id, "prpl-jabber") == 0)
        {
          gtk_widget_show (GTK_WIDGET (chatty_dialog->omemo.listbox_fp_own));
          gtk_widget_show (GTK_WIDGET (chatty_dialog->omemo.listbox_fp_own_dev));
          chatty_lurch_get_fp_list_own (self->selected_account);
        }

      settings_update_account_details (self);
    }
}

static void
settings_message_carbons_changed_cb (ChattySettingsDialog *self)
{
  gboolean active;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  active = gtk_switch_get_active (GTK_SWITCH (self->message_carbons_switch));
  purple_prefs_set_bool (CHATTY_PREFS_ROOT "/plugins/message_carbons", active);

  if (active)
    chatty_purple_load_plugin ("core-riba-carbons");
  else
    chatty_purple_unload_plugin ("core-riba-carbons");
}

static void
chatty_settings_back_clicked_cb (ChattySettingsDialog *self)
{
  const gchar *visible_child;

  visible_child = gtk_stack_get_visible_child_name (GTK_STACK (self->main_stack));

  if (g_str_equal (visible_child, "main-settings"))
    {
        gtk_widget_hide (GTK_WIDGET (self));
    }
  else
    {
      gtk_widget_hide (self->add_button);
      gtk_widget_hide (self->save_button);
      gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "main-settings");
    }
}

static char *
settings_show_dialog_load_avatar (void)
{
  GtkFileChooserNative *dialog;
  GtkWindow *window;
  char *file_name = NULL;
  int response;

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));

  dialog = gtk_file_chooser_native_new (_("Set Avatar"),
                                        window,
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("Open"),
                                        _("Cancel"));

  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog), getenv ("HOME"));

  // TODO: add preview widget when available in portrait mode

  response = gtk_native_dialog_run (GTK_NATIVE_DIALOG(dialog));

  if (response == GTK_RESPONSE_ACCEPT)
    file_name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(dialog));

  g_object_unref (dialog);

  return file_name;
}

static void
settings_avatar_button_clicked_cb (ChattySettingsDialog *self)
{
  g_autofree char *file_name = NULL;
  g_autoptr(GError) error = NULL;

  file_name = settings_show_dialog_load_avatar ();

  if (file_name)
    {
      PurplePluginProtocolInfo *prpl_info;
      GdkPixbuf *pixbuf, *origin_pixbuf;
      GtkWidget *avatar;
      const char *protocol_id;
      guchar *buffer;
      size_t len;

      protocol_id = purple_account_get_protocol_id (self->selected_account);
      prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO (purple_find_prpl (protocol_id));

      buffer = chatty_icon_get_data_from_pixbuf (file_name, prpl_info, &len);

      purple_buddy_icons_set_account_icon (self->selected_account, buffer, len);

      pixbuf = gdk_pixbuf_new_from_file (file_name, &error);

      if (error != NULL)
        {
          g_error ("Could not create pixbuf from file: %s", error->message);
          return;
        }

      avatar = gtk_image_new ();

      if (gdk_pixbuf_get_width (pixbuf) >= CHATTY_ICON_SIZE_LARGE ||
          gdk_pixbuf_get_height (pixbuf) >= CHATTY_ICON_SIZE_LARGE)
        {

          origin_pixbuf = g_object_ref (pixbuf);

          g_object_unref (pixbuf);

          pixbuf = gdk_pixbuf_scale_simple (origin_pixbuf,
                                            CHATTY_ICON_SIZE_LARGE,
                                            CHATTY_ICON_SIZE_LARGE,
                                            GDK_INTERP_BILINEAR);

          g_object_unref (origin_pixbuf);
        }

      gtk_image_set_from_pixbuf (GTK_IMAGE (avatar), chatty_icon_shape_pixbuf_circular (pixbuf));
      gtk_button_set_image (GTK_BUTTON (self->avatar_button), GTK_WIDGET (avatar));

      g_object_unref (pixbuf);
    }
}

static void
settings_account_id_changed_cb (ChattySettingsDialog *self,
                                GtkEntry             *account_id_entry)
{
  const gchar *id;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));
  g_assert (GTK_IS_ENTRY (account_id_entry));

  id = gtk_entry_get_text (account_id_entry);

  gtk_widget_set_sensitive (self->add_button, id && *id);
}

static void
settings_edit_password_clicked_cb (ChattySettingsDialog *self)
{
  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  gtk_widget_set_sensitive (self->save_button, TRUE);
  gtk_widget_set_sensitive (self->password_entry, TRUE);
  gtk_widget_grab_focus (self->password_entry);
}

static void chatty_settings_dialog_popuplate_account_list (ChattySettingsDialog *self);

static void
settings_delete_account_clicked_cb (ChattySettingsDialog *self)
{
  PurpleAccount *account;
  GtkWidget *dialog;
  int response;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  account = self->selected_account;
  dialog = gtk_message_dialog_new ((GtkWindow*)self,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_OK_CANCEL,
                                   _("Delete Account"));

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("Delete account %s?"),
                                            purple_account_get_username (account));

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);

  response = gtk_dialog_run (GTK_DIALOG(dialog));

  if (response == GTK_RESPONSE_OK)
    {
      self->selected_account = NULL;
      purple_accounts_delete (account);

      chatty_settings_dialog_popuplate_account_list (self);
      gtk_widget_hide (self->save_button);
      gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "main-settings");
    }

  gtk_widget_destroy (dialog);
}

static void
settings_protocol_changed_cb (ChattySettingsDialog *self,
                              GtkWidget            *button)
{
  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));
  g_assert (GTK_IS_TOGGLE_BUTTON (button));

  /* Show URL entry for non-Telegram accounts */
  gtk_widget_set_visible (self->server_url_entry, button != self->telegram_radio_button);

  if (button == self->xmpp_radio_button)
    gtk_entry_set_text (GTK_ENTRY (self->server_url_entry), "");
  else if (button == self->matrix_radio_button)
    gtk_entry_set_text (GTK_ENTRY (self->server_url_entry), "https://chat.librem.one");

  gtk_widget_grab_focus (self->account_id_entry);
}

static void
account_enabled_changed_cb (GtkListBoxRow *row,
                            GParamSpec    *pspec,
                            GtkSwitch     *enabled_switch)
{
  PurpleAccount *account;
  gboolean enabled;

  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_SWITCH (enabled_switch));

  account = g_object_get_data (G_OBJECT (row), "row-account");
  enabled = gtk_switch_get_active (enabled_switch);

  purple_account_set_enabled (account, CHATTY_UI, enabled);
}

static GtkWidget *
chatty_account_row_new (PurpleAccount *account)
{
  HdyActionRow   *row;
  GtkWidget      *account_enabled_switch;
  const gchar    *protocol_id;
  GtkWidget      *spinner;

  row = hdy_action_row_new ();
  gtk_widget_show (GTK_WIDGET (row));
  g_object_set_data (G_OBJECT(row),
                     "row-account",
                     (gpointer) account);

  protocol_id = purple_account_get_protocol_id (account);

  if ((g_strcmp0 (protocol_id, "prpl-jabber")) != 0 &&
      (g_strcmp0 (protocol_id, "prpl-matrix")) != 0 &&
      (g_strcmp0 (protocol_id, "prpl-telegram")) != 0 &&
      (g_strcmp0 (protocol_id, "prpl-delta")) != 0 &&
      (g_strcmp0 (protocol_id, "prpl-threepl")) != 0 &&
      (g_strcmp0 (protocol_id, "prpl-mm-sms")) != 0)
    return NULL;

  spinner = gtk_spinner_new ();
  gtk_widget_show (spinner);
  hdy_action_row_add_prefix (row, spinner);

  g_object_set_data (G_OBJECT(row),
                     "row-prefix",
                     (gpointer)spinner);

  account_enabled_switch = gtk_switch_new ();
  gtk_widget_show (account_enabled_switch);

  g_object_set  (G_OBJECT(account_enabled_switch),
                 "valign", GTK_ALIGN_CENTER,
                 "halign", GTK_ALIGN_END,
                 NULL);

  gtk_switch_set_state (GTK_SWITCH(account_enabled_switch),
                        purple_account_get_enabled (account, CHATTY_UI));

  g_signal_connect_object (account_enabled_switch,
                           "notify::active",
                           G_CALLBACK(account_enabled_changed_cb),
                           (gpointer) row,
                           G_CONNECT_SWAPPED);

  hdy_action_row_set_title (row, purple_account_get_username (account));
  hdy_action_row_set_subtitle (row, purple_account_get_protocol_name (account));
  hdy_action_row_add_action (row, account_enabled_switch);
  hdy_action_row_set_activatable_widget (row, NULL);

  return GTK_WIDGET (row);
}

static void
chatty_settings_dialog_popuplate_account_list (ChattySettingsDialog *self)
{
  gint index = 0;

  chatty_account_list_clear (self, GTK_LIST_BOX (self->accounts_list_box));

  for (GList *l = purple_accounts_get_all (); l != NULL; l = l->next) {
    GtkWidget *row;
    const gchar *protocol_id;

    protocol_id = purple_account_get_protocol_id ((PurpleAccount *)l->data);

    if (g_strcmp0 (protocol_id, "prpl-mm-sms") == 0)
      continue;

    row = chatty_account_row_new (l->data);

    if (!row)
      continue;

    gtk_list_box_insert (GTK_LIST_BOX (self->accounts_list_box), row, index);
    index++;
  }
}


void
chatty_settings_update_accounts (GObject *object) 
{
  ChattySettingsDialog *self = (ChattySettingsDialog *)object;

  g_return_if_fail (CHATTY_IS_SETTINGS_DIALOG(self));

  chatty_settings_dialog_popuplate_account_list (self);
}


static void
chatty_settings_dialog_constructed (GObject *object)
{
  ChattySettingsDialog *self = (ChattySettingsDialog *)object;
  ChattySettings *settings;
  chatty_data_t  *chatty;
  chatty_dialog_data_t *chatty_dialog;

  G_OBJECT_CLASS (chatty_settings_dialog_parent_class)->constructed (object);

  chatty   = chatty_get_data ();
  chatty_dialog = chatty_get_dialog_data ();

  settings = chatty_settings_get_default ();
  self->settings = g_object_ref (settings);
  chatty->list_manage_account = GTK_LIST_BOX (self->accounts_list_box);

  chatty_dialog->omemo.listbox_fp_own = GTK_LIST_BOX (self->fingerprint_list);
  chatty_dialog->omemo.listbox_fp_own_dev = GTK_LIST_BOX (self->fingerprint_device_list);

  g_object_bind_property (settings, "send-receipts",
                          self->send_receipts_switch, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (settings, "send-typing",
                          self->typing_notification_switch, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (settings, "greyout-offline-buddies",
                          self->indicate_offline_switch, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (settings, "blur-idle-buddies",
                          self->indicate_idle_switch, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (settings, "indicate-unknown-contacts",
                          self->indicate_unknown_switch, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (settings, "convert-emoticons",
                          self->convert_smileys_switch, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (settings, "return-sends-message",
                          self->return_sends_switch, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  chatty_settings_dialog_popuplate_account_list (self);
}

static void
chatty_settings_dialog_finalize (GObject *object)
{
  ChattySettingsDialog *self = (ChattySettingsDialog *)object;
  chatty_data_t  *chatty;

  chatty = chatty_get_data ();

  g_clear_object (&self->settings);
  chatty->list_manage_account = NULL;

  G_OBJECT_CLASS (chatty_settings_dialog_parent_class)->finalize (object);
}

static void
chatty_settings_dialog_class_init (ChattySettingsDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed  = chatty_settings_dialog_constructed;
  object_class->finalize = chatty_settings_dialog_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/chatty/"
                                               "ui/chatty-settings-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, add_button);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, save_button);

  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, main_stack);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, accounts_list_box);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, add_account_row);

  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, avatar_button);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, account_id_entry);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, account_protocol_label);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, status_label);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, password_entry);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, edit_password_button);

  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, protocol_list);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, protocol_title_label);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, xmpp_radio_button);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, matrix_row);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, matrix_radio_button);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, telegram_row);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, telegram_radio_button);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, new_account_settings_list);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, new_account_id_entry);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, server_url_entry);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, new_password_entry);

  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, fingerprint_list);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, fingerprint_device_list);

  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, send_receipts_switch);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, message_carbons_row);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, message_carbons_switch);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, typing_notification_switch);

  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, indicate_offline_switch);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, indicate_idle_switch);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, indicate_unknown_switch);

  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, convert_smileys_switch);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, return_sends_switch);

  gtk_widget_class_bind_template_callback (widget_class, chatty_settings_add_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, chatty_settings_save_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, account_list_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, settings_message_carbons_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, chatty_settings_back_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, settings_avatar_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, settings_account_id_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, settings_edit_password_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, settings_delete_account_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, settings_protocol_changed_cb);
}

static void
chatty_settings_dialog_init (ChattySettingsDialog *self)
{
  chatty_purple_data_t *chatty_purple;

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->accounts_list_box),
                                hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (self->fingerprint_list),
                                hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (self->fingerprint_device_list),
                                hdy_list_box_separator_header, NULL, NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->protocol_list),
                                hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (self->new_account_settings_list),
                                hdy_list_box_separator_header, NULL, NULL);

  chatty_purple = chatty_get_purple_data ();
  gtk_widget_set_visible (self->message_carbons_row,
                          chatty_purple->plugin_carbons_available);
  gtk_switch_set_state (GTK_SWITCH (self->message_carbons_switch),
                        chatty_purple->plugin_carbons_loaded);
}

GtkWidget *
chatty_settings_dialog_new (GtkWindow *parent_window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (parent_window), NULL);

  return g_object_new (CHATTY_TYPE_SETTINGS_DIALOG,
                       "transient-for", parent_window,
                       "use-header-bar", 1,
                       NULL);
}
