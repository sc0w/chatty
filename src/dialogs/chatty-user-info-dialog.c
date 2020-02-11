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
#include "chatty-dialogs.h"
#include "chatty-manager.h"
#include "users/chatty-pp-account.h"
#include "chatty-utils.h"
#include "chatty-buddy-list.h"
#include "chatty-conversation.h"
#include "chatty-icons.h"
#include "chatty-purple-init.h"
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

  ChattyConversation *chatty_conv;
  PurpleBuddy        *buddy;
  const char         *alias;
};


enum {
  PROP_0,
  PROP_CHATTY_CONV,
  PROP_LAST
};

static GParamSpec *props[PROP_LAST];

G_DEFINE_TYPE (ChattyUserInfoDialog, chatty_user_info_dialog, HDY_TYPE_DIALOG)


static void chatty_user_info_dialog_update_avatar (ChattyUserInfoDialog *self, const char *color);
static void chatty_user_info_dialog_set_encrypt (ChattyUserInfoDialog *self, gboolean active);
static void chatty_info_dialog_get_encrypt_status (ChattyUserInfoDialog *self);


static void
button_avatar_clicked_cb (ChattyUserInfoDialog *self)
{
  PurpleContact *contact;
  char          *file_name = NULL;
  
  file_name = chatty_dialogs_show_dialog_load_avatar ();

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
switch_encryption_changed_cb (ChattyUserInfoDialog *self)
{
  gboolean active;

  g_assert (CHATTY_IS_USER_INFO_DIALOG (self));

  active = gtk_switch_get_active (GTK_SWITCH(self->switch_encrypt));

  chatty_user_info_dialog_set_encrypt (self, active ? TRUE : FALSE);

  chatty_info_dialog_get_encrypt_status (self);
}


static void
encrypt_set_enable_cb (int      err,
                       gpointer user_data)
{
  ChattyUserInfoDialog *self = (ChattyUserInfoDialog *)user_data;

  if (err) {
    g_debug ("Failed to enable OMEMO for this conversation.");
    return;
  }

  gtk_switch_set_state (GTK_SWITCH(self->switch_encrypt), TRUE);

  self->chatty_conv->omemo.enabled = TRUE;
}


static void
encrypt_set_disable_cb (int      err,
                        gpointer user_data)
{
  ChattyUserInfoDialog *self = (ChattyUserInfoDialog *)user_data;

  if (err) {
    g_debug ("Failed to disable OMEMO for this conversation.");
    return;
  }

  gtk_switch_set_state (GTK_SWITCH(self->switch_encrypt), FALSE);
  
  self->chatty_conv->omemo.enabled = FALSE;
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
encrypt_status_cb (int      err,
                   int      status,
                   gpointer user_data)
{
  ChattyUserInfoDialog *self = (ChattyUserInfoDialog *)user_data;
  GtkStyleContext      *sc;
  const char           *status_msg;

  if (err) {
    g_debug ("Failed to get the OMEMO status.");
    return;
  }

  sc = gtk_widget_get_style_context (GTK_WIDGET(self->chatty_conv->omemo.symbol_encrypt));

  switch (status) {
    case LURCH_STATUS_DISABLED:
      status_msg = _("This chat is not encrypted");
      break;
    case LURCH_STATUS_NOT_SUPPORTED:
      status_msg = _("Encryption is not available");
      break;
    case LURCH_STATUS_NO_SESSION:
      status_msg = _("This chat is not encrypted");
      self->chatty_conv->omemo.enabled = FALSE;
      break;
    case LURCH_STATUS_OK:
      status_msg = _("This chat is encrypted");
      self->chatty_conv->omemo.enabled = TRUE;
      break;
    default:
      g_warning ("Received unknown status code.");
      return;
  }

  gtk_image_set_from_icon_name (self->chatty_conv->omemo.symbol_encrypt,
                                self->chatty_conv->omemo.enabled ? "changes-prevent-symbolic" :
                                                             "changes-allow-symbolic",
                                1);

  gtk_style_context_remove_class (sc, self->chatty_conv->omemo.enabled ? "unencrypt" : "encrypt");
  gtk_style_context_add_class (sc, self->chatty_conv->omemo.enabled ? "encrypt" : "unencrypt");

  gtk_label_set_text (GTK_LABEL(self->label_encrypt_status), status_msg);
}


static void
chatty_user_info_dialog_set_encrypt (ChattyUserInfoDialog *self,
                                     gboolean              active)
{
  PurpleAccount          *account;
  PurpleConversationType  type;
  const char             *name;

  account = purple_conversation_get_account (self->chatty_conv->conv);
  type = purple_conversation_get_type (self->chatty_conv->conv);
  name = purple_conversation_get_name (self->chatty_conv->conv);

  if (type == PURPLE_CONV_TYPE_IM) {
    purple_signal_emit (purple_plugins_get_handle(),
                        active ? "lurch-enable-im" : "lurch-disable-im",
                        account,
                        chatty_utils_jabber_id_strip (name),
                        active ? encrypt_set_enable_cb : encrypt_set_disable_cb,
                        self);
  }
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
chatty_info_dialog_get_encrypt_status (ChattyUserInfoDialog *self)
{
  PurpleAccount          *account;
  PurpleConversationType  type;
  const char             *name;

  account = purple_conversation_get_account (self->chatty_conv->conv);
  type = purple_conversation_get_type (self->chatty_conv->conv);
  name = purple_conversation_get_name (self->chatty_conv->conv);

  if (type == PURPLE_CONV_TYPE_IM) {
    g_autofree char *stripped = chatty_utils_jabber_id_strip (name);
    purple_signal_emit (purple_plugins_get_handle(),
                        "lurch-status-im",
                        account,
                        stripped,
                        encrypt_status_cb,
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
chatty_user_info_dialog_set_property (GObject      *object,
                                      guint         property_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ChattyUserInfoDialog *self = (ChattyUserInfoDialog *)object;

  switch (property_id) {
    case PROP_CHATTY_CONV:
      self->chatty_conv = g_value_get_pointer (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}


static void
chatty_user_info_dialog_get_property (GObject      *object,
                                      guint         property_id,
                                      GValue       *value,
                                      GParamSpec   *pspec)
{
  ChattyUserInfoDialog *self = (ChattyUserInfoDialog *)object;

  switch (property_id) {
    case PROP_CHATTY_CONV:
      g_value_set_pointer (value, self->chatty_conv);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}


static void
chatty_user_info_dialog_constructed (GObject *object)
{
  ChattyManager  *manager;
  PurpleAccount  *account;
  PurplePresence *presence;
  PurpleStatus   *status;
  const char     *protocol_id;

  ChattyUserInfoDialog *self = (ChattyUserInfoDialog *)object;

  G_OBJECT_CLASS (chatty_user_info_dialog_parent_class)->constructed (object);

  account = purple_conversation_get_account (self->chatty_conv->conv);
  protocol_id = purple_account_get_protocol_id (account);

  manager = chatty_manager_get_default ();

  if (chatty_manager_lurch_plugin_is_loaded (manager) && (!g_strcmp0 (protocol_id, "prpl-jabber"))) {

    gtk_widget_show (GTK_WIDGET(self->label_status_msg));
    gtk_widget_show (GTK_WIDGET(self->label_encrypt));
    gtk_widget_show (GTK_WIDGET(self->label_encrypt_status));
    gtk_widget_show (GTK_WIDGET(self->listbox_prefs));

    chatty_info_dialog_get_encrypt_status (self);
    chatty_user_info_dialog_request_fps (self);

    gtk_switch_set_state (GTK_SWITCH(self->switch_encrypt), self->chatty_conv->omemo.enabled);
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
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

  object_class->constructed  = chatty_user_info_dialog_constructed;

  object_class->set_property = chatty_user_info_dialog_set_property;
  object_class->get_property = chatty_user_info_dialog_get_property;

  props[PROP_CHATTY_CONV] =
    g_param_spec_pointer ("chatty-conv",
                          "CHATTY_CONVERSATION",
                          "A Chatty conversation",
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST, props);

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
  gtk_widget_class_bind_template_callback (widget_class, switch_encryption_changed_cb);
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
chatty_user_info_dialog_new (GtkWindow *parent_window,
                             gpointer   chatty_conv)
{
  g_return_val_if_fail (chatty_conv != NULL, NULL);

  return g_object_new (CHATTY_TYPE_USER_INFO_DIALOG,
                       "transient-for", parent_window,
                       "chatty-conv", chatty_conv,
                       "use-header-bar", 1,
                       NULL);
}
