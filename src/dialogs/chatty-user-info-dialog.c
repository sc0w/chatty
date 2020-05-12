/*
 * Copyright (C) 2020 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */



#define G_LOG_DOMAIN "chatty-user-info-dialog"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include "chatty-window.h"
#include "chatty-manager.h"
#include "users/chatty-pp-account.h"
#include "chatty-utils.h"
#include "chatty-conversation.h"
#include "chatty-icons.h"
#include "chatty-enums.h"
#include "chatty-chat.h"
#include "chatty-user-info-dialog.h"


struct _ChattyUserInfoDialog
{
  HdyDialog  parent_instance;

  GtkWidget *label_alias;
  GtkWidget *label_jid;
  GtkWidget *label_user_id;
  GtkWidget *label_user_status;
  GtkWidget *label_status_msg;
  GtkWidget *switch_notify;
  GtkWidget *listbox_prefs;
  GtkWidget *button_avatar;
  GtkWidget *switch_encrypt;
  GtkWidget *label_encrypt;
  GtkWidget *label_encrypt_status;
  GtkWidget *listbox_fps;

  ChattyChat *chat;
  ChattyConversation *chatty_conv;
  PurpleBuddy        *buddy;
  const char         *alias;
};


G_DEFINE_TYPE (ChattyUserInfoDialog, chatty_user_info_dialog, HDY_TYPE_DIALOG)


static void chatty_user_info_dialog_update_avatar (ChattyUserInfoDialog *self, const char *color);


/* Copied from chatty-dialogs.c written by Andrea Schäfer <mosibasu@me.com> */
static char *
show_select_avatar_dialog (ChattyUserInfoDialog *self)
{
  GtkFileChooserNative *dialog;
  gchar                *file_name;
  int                   response;

  dialog = gtk_file_chooser_native_new (_("Set Avatar"),
                                        GTK_WINDOW (self),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("Open"),
                                        _("Cancel"));

  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), getenv ("HOME"));

  // TODO: add preview widget when available in portrait mode

  response = gtk_native_dialog_run (GTK_NATIVE_DIALOG (dialog));

  if (response == GTK_RESPONSE_ACCEPT) {
    file_name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
  } else {
    file_name = NULL;
  }

  g_object_unref (dialog);

  return file_name;
}


static void
button_avatar_clicked_cb (ChattyUserInfoDialog *self)
{
  PurpleContact *contact;
  char          *file_name = NULL;
  
  file_name = show_select_avatar_dialog (self);

  if (file_name) {
    contact = purple_buddy_get_contact (self->buddy);

    purple_buddy_icons_node_set_custom_icon_from_file ((PurpleBlistNode*)contact, file_name);

    chatty_user_info_dialog_update_avatar (self, CHATTY_COLOR_BLUE);
  }

  g_free (file_name);
}


static void
switch_notify_changed_cb (ChattyUserInfoDialog *self)
{
  gboolean active;

  g_assert (CHATTY_IS_USER_INFO_DIALOG (self));

  active = gtk_switch_get_active (GTK_SWITCH(self->switch_notify));

  purple_blist_node_set_bool (PURPLE_BLIST_NODE(self->buddy), 
                              "chatty-notifications", 
                              active);
}


static void
list_fps_changed_cb (ChattyUserInfoDialog *self)
{
  if (gtk_list_box_get_row_at_index (GTK_LIST_BOX(self->listbox_fps), 0)) {

    gtk_widget_show (GTK_WIDGET(self->listbox_fps));
  } else {
    gtk_widget_hide (GTK_WIDGET(self->listbox_fps));
  }
}

static void
encrypt_fp_list_cb (int         err,
                    GHashTable *id_fp_table,
                    gpointer    user_data)
{
  GList       *key_list = NULL;
  const GList *curr_p = NULL;
  const char  *fp = NULL;
  GtkWidget   *row;

  ChattyUserInfoDialog *self = (ChattyUserInfoDialog *)user_data;

  if (err || !id_fp_table) {
    gtk_widget_hide (GTK_WIDGET(self->listbox_fps));
    gtk_label_set_text (GTK_LABEL(self->label_encrypt_status), _("Encryption not available"));

    return;
  }

  if (self->listbox_fps) {
    key_list = g_hash_table_get_keys(id_fp_table);

    for (curr_p = key_list; curr_p; curr_p = curr_p->next) {
      fp = (char *) g_hash_table_lookup(id_fp_table, curr_p->data);

      g_debug ("DeviceId: %i fingerprint:\n%s\n", *((guint32 *) curr_p->data),
               fp ? fp : "(no session)");

      row = chatty_utils_create_fingerprint_row (fp, *((guint32 *) curr_p->data));

      if (row) {
        gtk_container_add (GTK_CONTAINER(self->listbox_fps), row);
      }
    }
  }

  g_list_free (key_list);
}


static void
user_info_dialog_encrypt_changed_cb (ChattyUserInfoDialog *self)
{
  const char *status_msg;
  ChattyEncryption encryption;

  g_assert (CHATTY_IS_USER_INFO_DIALOG (self));

  encryption = chatty_chat_get_encryption_status (self->chat);

  if (encryption == CHATTY_ENCRYPTION_UNSUPPORTED ||
      encryption == CHATTY_ENCRYPTION_UNKNOWN)
    status_msg = _("Encryption is not available");
  else if (encryption == CHATTY_ENCRYPTION_ENABLED)
    status_msg = _("This chat is encrypted");
  else if (encryption == CHATTY_ENCRYPTION_DISABLED)
    status_msg = _("This chat is not encrypted");
  else
    g_return_if_reached ();

  gtk_label_set_text (GTK_LABEL(self->label_encrypt_status), status_msg);
}

static void
chatty_user_info_dialog_request_fps (ChattyUserInfoDialog *self)
{
  PurpleAccount          *account;
  PurpleConversationType  type;
  const char             *name;

  void * plugins_handle = purple_plugins_get_handle();

  account = purple_conversation_get_account (self->chatty_conv->conv);
  type = purple_conversation_get_type (self->chatty_conv->conv);
  name = purple_conversation_get_name (self->chatty_conv->conv);

  if (type == PURPLE_CONV_TYPE_IM) {
    purple_signal_emit (plugins_handle,
                        "lurch-fp-other",
                        account,
                        chatty_utils_jabber_id_strip (name),
                        encrypt_fp_list_cb,
                        self);
  }
}

static void 
chatty_user_info_dialog_update_avatar (ChattyUserInfoDialog *self,
                                       const char           *color)
{
  GtkWindow     *window;
  PurpleContact *contact;
  GdkPixbuf     *icon;
  GtkWidget     *avatar;
  const char    *buddy_alias;
  const char    *contact_alias;

  icon = chatty_icon_get_buddy_icon (PURPLE_BLIST_NODE(self->buddy),
                                     self->alias,
                                     CHATTY_ICON_SIZE_LARGE,
                                     color,
                                     FALSE);
  
  if (icon != NULL) {
    avatar = gtk_image_new ();
    gtk_image_set_from_pixbuf (GTK_IMAGE(avatar), icon);
    gtk_button_set_image (GTK_BUTTON(self->button_avatar), GTK_WIDGET(avatar));
  }

  g_object_unref (icon);

  contact = purple_buddy_get_contact (self->buddy);
  buddy_alias = purple_buddy_get_alias (self->buddy);
  contact_alias = purple_contact_get_alias (contact);

  icon = chatty_icon_get_buddy_icon (PURPLE_BLIST_NODE(self->buddy),
                                     self->alias,
                                     CHATTY_ICON_SIZE_SMALL,
                                     color,
                                     FALSE);

  window = gtk_window_get_transient_for (GTK_WINDOW (self));

  chatty_window_update_sub_header_titlebar ((ChattyWindow *)window,
                                            icon, 
                                            contact_alias ? contact_alias : buddy_alias);

  g_object_unref (icon);
}


static void
chatty_user_info_dialog_update_chat (ChattyUserInfoDialog *self)
{
  ChattyManager  *manager;
  PurpleAccount  *account;
  PurplePresence *presence;
  PurpleStatus   *status;
  const char     *protocol_id;

  g_assert (CHATTY_IS_USER_INFO_DIALOG (self));

  account = purple_conversation_get_account (self->chatty_conv->conv);
  protocol_id = purple_account_get_protocol_id (account);

  manager = chatty_manager_get_default ();

  g_object_bind_property (self->chat, "encrypt",
                          self->switch_encrypt, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  g_signal_connect_object (self->chat, "notify::encrypt",
                           G_CALLBACK (user_info_dialog_encrypt_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  if (chatty_manager_lurch_plugin_is_loaded (manager) && (!g_strcmp0 (protocol_id, "prpl-jabber"))) {

    gtk_widget_show (GTK_WIDGET(self->label_status_msg));
    gtk_widget_show (GTK_WIDGET(self->label_encrypt));
    gtk_widget_show (GTK_WIDGET(self->label_encrypt_status));

    chatty_chat_load_encryption_status (self->chat);
    chatty_user_info_dialog_request_fps (self);
  }

  self->buddy = purple_find_buddy (self->chatty_conv->conv->account, self->chatty_conv->conv->name);
  self->alias = purple_buddy_get_alias (self->buddy);

  if (chatty_blist_protocol_is_sms (account)) {
    chatty_user_info_dialog_update_avatar (self, CHATTY_COLOR_GREEN);

    gtk_label_set_text (GTK_LABEL(self->label_user_id), _("Phone Number:"));
  } else {
    chatty_user_info_dialog_update_avatar (self, CHATTY_COLOR_BLUE);
  }

  if (!g_strcmp0 (protocol_id, "prpl-jabber")) {

    gtk_widget_show (GTK_WIDGET(self->label_user_status));
    gtk_widget_show (GTK_WIDGET(self->label_status_msg));

    presence = purple_buddy_get_presence (self->buddy);
    status = purple_presence_get_active_status (presence);

    gtk_label_set_text (GTK_LABEL(self->label_user_id), "XMPP ID");
    gtk_label_set_text (GTK_LABEL(self->label_status_msg), purple_status_get_name (status));
  }

  gtk_switch_set_state (GTK_SWITCH(self->switch_notify),
                        purple_blist_node_get_bool (PURPLE_BLIST_NODE(self->buddy),
                        "chatty-notifications"));

  gtk_label_set_text (GTK_LABEL(self->label_alias), chatty_utils_jabber_id_strip (self->alias));
  gtk_label_set_text (GTK_LABEL(self->label_jid), self->chatty_conv->conv->name);
}


static void
chatty_user_info_dialog_class_init (ChattyUserInfoDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/chatty/"
                                               "ui/chatty-dialog-user-info.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyUserInfoDialog, button_avatar);
  gtk_widget_class_bind_template_child (widget_class, ChattyUserInfoDialog, label_user_id);
  gtk_widget_class_bind_template_child (widget_class, ChattyUserInfoDialog, label_alias);
  gtk_widget_class_bind_template_child (widget_class, ChattyUserInfoDialog, label_jid);
  gtk_widget_class_bind_template_child (widget_class, ChattyUserInfoDialog, label_user_status);
  gtk_widget_class_bind_template_child (widget_class, ChattyUserInfoDialog, label_status_msg);
  gtk_widget_class_bind_template_child (widget_class, ChattyUserInfoDialog, switch_notify);
  gtk_widget_class_bind_template_child (widget_class, ChattyUserInfoDialog, label_encrypt);
  gtk_widget_class_bind_template_child (widget_class, ChattyUserInfoDialog, label_encrypt_status);
  gtk_widget_class_bind_template_child (widget_class, ChattyUserInfoDialog, switch_encrypt);
  gtk_widget_class_bind_template_child (widget_class, ChattyUserInfoDialog, listbox_prefs);
  gtk_widget_class_bind_template_child (widget_class, ChattyUserInfoDialog, listbox_fps);

  gtk_widget_class_bind_template_callback (widget_class, button_avatar_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, switch_notify_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, list_fps_changed_cb);
}


static void
chatty_user_info_dialog_init (ChattyUserInfoDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET(self));

  gtk_list_box_set_header_func (GTK_LIST_BOX(self->listbox_fps),
                                hdy_list_box_separator_header,
                                NULL, NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX(self->listbox_prefs),
                                hdy_list_box_separator_header,
                                NULL, NULL);
}


GtkWidget *
chatty_user_info_dialog_new (GtkWindow *parent_window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (parent_window), NULL);

  return g_object_new (CHATTY_TYPE_USER_INFO_DIALOG,
                       "transient-for", parent_window,
                       "use-header-bar", 1,
                       NULL);
}

void
chatty_user_info_dialog_set_chat (ChattyUserInfoDialog *self,
                                  ChattyChat           *chat)
{
  PurpleConversation *conv;

  g_return_if_fail (CHATTY_IS_USER_INFO_DIALOG (self));
  g_return_if_fail (CHATTY_IS_CHAT (chat));

  conv = chatty_chat_get_purple_conv (chat);
  self->chat = chat;
  self->chatty_conv = conv->ui_data;

  chatty_user_info_dialog_update_chat (self);
}
