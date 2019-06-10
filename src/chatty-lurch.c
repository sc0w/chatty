/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-lurch"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "purple.h"
#include "chatty-utils.h"
#include "chatty-dialogs.h"
#include "chatty-lurch.h"
#include "chatty-conversation.h"


static GtkWidget* chatty_lurch_create_fingerprint_row (const char *fp, guint id);


static void
cb_get_fingerprint (int         err,
                    const char *fp,
                    gpointer    user_data)
{
  PurpleConversation *conv;
  ChattyConversation *chatty_conv;

  if (err) {
    g_debug ("Failed to get fingerprint from own device.");
    return;
  }

  conv = (PurpleConversation *) user_data;
  chatty_conv = CHATTY_CONVERSATION(conv);

  chatty_conv->omemo.fp_own_device = fp;
}


static void
cb_get_fp_list_own (int         err,
                    GHashTable *id_fp_table,
                    gpointer    user_data)
{
  GList       *key_list = NULL;
  GList       *filtered_list = NULL;
  const GList *curr_p = NULL;
  const char  *fp = NULL;
  GtkWidget   *row;

  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  if (err || !id_fp_table) {
    gtk_widget_hide (GTK_WIDGET(chatty_dialog->omemo.listbox_fp_own));
    gtk_widget_hide (GTK_WIDGET(chatty_dialog->omemo.listbox_fp_own_dev));

    return;
  }

  if (chatty_dialog->omemo.listbox_fp_own &&
      chatty_dialog->omemo.listbox_fp_own_dev) {

    key_list = g_hash_table_get_keys (id_fp_table);

    for (curr_p = key_list; curr_p; curr_p = curr_p->next) {
      fp = (char *) g_hash_table_lookup(id_fp_table, curr_p->data);

      if (fp) {
        filtered_list = g_list_append (filtered_list, curr_p->data);
      }
    }

    for (curr_p = filtered_list; curr_p; curr_p = curr_p->next) {
      fp = (char *) g_hash_table_lookup(id_fp_table, curr_p->data);

      g_debug ("DeviceId: %i fingerprint:\n%s\n", *((guint32 *) curr_p->data),
               fp ? fp : "(no session)");

      row = chatty_lurch_create_fingerprint_row (fp, *((guint32 *) curr_p->data));

      if (row) {
        if (curr_p == g_list_last (filtered_list)) {
          gtk_container_add (GTK_CONTAINER(chatty_dialog->omemo.listbox_fp_own), row);
        } else {
          gtk_container_add (GTK_CONTAINER(chatty_dialog->omemo.listbox_fp_own_dev), row);
        }
      }
    }

    if (g_list_length (filtered_list) == 1) {
      gtk_widget_hide (GTK_WIDGET(chatty_dialog->omemo.listbox_fp_own_dev));
    }
  }

  g_list_free (key_list);
  g_list_free (filtered_list);
}


static void
cb_get_fp_list_contact (int         err,
                        GHashTable *id_fp_table,
                        gpointer    user_data)
{
  GList       *key_list = NULL;
  const GList *curr_p = NULL;
  const char  *fp = NULL;
  GtkWidget   *row;

  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  if (err || !id_fp_table) {
    gtk_widget_hide (GTK_WIDGET(chatty_dialog->omemo.listbox_fp_contact));
    gtk_label_set_text (GTK_LABEL(chatty_dialog->omemo.label_status_msg), _("Encryption not available"));

    return;
  }

  if (chatty_dialog->omemo.listbox_fp_contact) {
    key_list = g_hash_table_get_keys(id_fp_table);

    for (curr_p = key_list; curr_p; curr_p = curr_p->next) {
      fp = (char *) g_hash_table_lookup(id_fp_table, curr_p->data);

      g_debug ("DeviceId: %i fingerprint:\n%s\n", *((guint32 *) curr_p->data),
               fp ? fp : "(no session)");

      row = chatty_lurch_create_fingerprint_row (fp, *((guint32 *) curr_p->data));

      if (row) {
        gtk_container_add (GTK_CONTAINER(chatty_dialog->omemo.listbox_fp_contact), row);
      }
    }
  }

  g_list_free (key_list);
}


static void
cb_set_enable (int      err,
               gpointer user_data)
{
  PurpleConversation *conv;
  ChattyConversation *chatty_conv;

  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  if (err) {
    g_debug ("Failed to enable OMEMO for this conversation.");
    return;
  }

  conv = (PurpleConversation *) user_data;
  chatty_conv = CHATTY_CONVERSATION(conv);

  gtk_switch_set_state (chatty_dialog->omemo.switch_on_off, TRUE);
  chatty_conv->omemo.enabled = TRUE;
}


static void
cb_set_disable (int      err,
                gpointer user_data)
{
  PurpleConversation *conv;
  ChattyConversation *chatty_conv;

  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  if (err) {
    g_debug ("Failed to disable OMEMO for this conversation.");
    return;
  }

  conv = (PurpleConversation *) user_data;
  chatty_conv = CHATTY_CONVERSATION(conv);

  gtk_switch_set_state (chatty_dialog->omemo.switch_on_off, FALSE);
  chatty_conv->omemo.enabled = FALSE;
}


static void
cb_get_status (int      err,
               int      status,
               gpointer user_data)
{
  PurpleConversation *conv = (PurpleConversation *) user_data;
  ChattyConversation *chatty_conv;
  GtkStyleContext    *sc;
  const char         *status_msg;

  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  if (err) {
    g_debug ("Failed to get the OMEMO status.");
    return;
  }

  chatty_conv = CHATTY_CONVERSATION(conv);

  sc = gtk_widget_get_style_context (GTK_WIDGET(chatty_conv->omemo.symbol_encrypt));

  switch (status) {
    case LURCH_STATUS_DISABLED:
      status_msg = _("This chat is not encrypted");
      break;
    case LURCH_STATUS_NOT_SUPPORTED:
      status_msg = _("Encryption is not available");
      break;
    case LURCH_STATUS_NO_SESSION:
      status_msg = _("This chat is not encrypted");
      chatty_conv->omemo.enabled = FALSE;
      break;
    case LURCH_STATUS_OK:
      status_msg = _("This chat is encrypted");
      chatty_conv->omemo.enabled = TRUE;
      break;
    default:
      g_debug ("Received unknown status code.");
  }

  gtk_image_set_from_icon_name (chatty_conv->omemo.symbol_encrypt,
                                chatty_conv->omemo.enabled ? "changes-prevent-symbolic" :
                                                             "changes-allow-symbolic",
                                1);

  gtk_style_context_remove_class (sc, chatty_conv->omemo.enabled ? "unencrypt" : "encrypt");
  gtk_style_context_add_class (sc, chatty_conv->omemo.enabled ? "encrypt" : "unencrypt");

  if (chatty_dialog->omemo.label_status_msg) {
    gtk_label_set_text (GTK_LABEL(chatty_dialog->omemo.label_status_msg), status_msg);
  }

  chatty_conv->omemo.status = status;
}


static GtkWidget*
chatty_lurch_create_fingerprint_row (const char *fp,
                                     guint       id)
{
  GtkWidget     *row;
  GtkBox        *vbox;
  GtkLabel      *label_fp;
  GtkLabel      *label_id;
  g_auto(GStrv)  line_split = NULL;
  char          *markup_fp = NULL;
  char          *markup_id = NULL;
  char          *device_id;

  if (!fp) {
    return NULL;
  }

  line_split = g_strsplit (fp, " ", -1);

  markup_fp = "<span font_family='monospace' font='9'>";

  for (int i = 0; i < 8; i++) {
    markup_fp = g_strconcat (markup_fp,
                             i % 2 ? "<span color='DarkGrey'>"
                                   : "<span color='DimGrey'>",
                             line_split[i],
                             i == 3 ? "\n" : " ",
                             "</span>",
                             i == 7 ? "</span>" : "\0",
                             NULL);
  }

  device_id = g_strdup_printf ("%i", id);

  markup_id = g_strconcat ("<span font='9'>"
                           "<span color='DimGrey'>",
                           "Device ID ",
                           device_id,
                           " fingerprint:",
                           "</span></span>",
                           NULL);

  row = GTK_WIDGET(gtk_list_box_row_new ());
  g_object_set (G_OBJECT(row),
                "selectable", FALSE,
                "activatable", FALSE,
                NULL);

  vbox = GTK_BOX(gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  g_object_set (G_OBJECT(vbox),
                "margin_top", 6,
                "margin_bottom", 6,
                "margin_start", 12,
                "margin_end", 6,
                NULL);

  label_id = GTK_LABEL(gtk_label_new (NULL));
  gtk_label_set_markup (GTK_LABEL(label_id), g_strdup (markup_id));
  g_object_set (G_OBJECT(label_id),
                "can_focus", FALSE,
                "use_markup", TRUE,
                "ellipsize", PANGO_ELLIPSIZE_END,
                "halign", GTK_ALIGN_START,
                "hexpand", TRUE,
                "xalign", 0,
                NULL);

  label_fp = GTK_LABEL(gtk_label_new (NULL));
  gtk_label_set_markup (GTK_LABEL(label_fp), g_strdup (markup_fp));
  g_object_set (G_OBJECT(label_fp),
                "can_focus", FALSE,
                "use_markup", TRUE,
                "ellipsize", PANGO_ELLIPSIZE_END,
                "halign", GTK_ALIGN_START,
                "hexpand", TRUE,
                "margin_top", 8,
                "xalign", 0,
                NULL);

  gtk_box_pack_start (vbox, GTK_WIDGET(label_id), FALSE, FALSE, 0);
  gtk_box_pack_start (vbox, GTK_WIDGET(label_fp), FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER(row), GTK_WIDGET(vbox));
  gtk_widget_show_all (GTK_WIDGET(row));

  g_free (markup_fp);
  g_free (markup_id);
  g_free (device_id);

  return GTK_WIDGET(row);
}


void
chatty_lurch_get_fp_list_own (PurpleAccount *account)
{
  void * plugins_handle = purple_plugins_get_handle();

  purple_signal_emit (plugins_handle,
                      "lurch-fp-list",
                      account,
                      cb_get_fp_list_own,
                      NULL);
}


void
chatty_lurch_get_fp_list_contact (PurpleConversation *conv)
{
  PurpleAccount          *account;
  PurpleConversationType  type;
  const char             *name;

  void * plugins_handle = purple_plugins_get_handle();

  account = purple_conversation_get_account (conv);
  type = purple_conversation_get_type (conv);
  name = purple_conversation_get_name (conv);

  if (type == PURPLE_CONV_TYPE_IM) {
    purple_signal_emit (plugins_handle,
                        "lurch-fp-other",
                        account,
                        chatty_utils_jabber_id_strip (name),
                        cb_get_fp_list_contact,
                        conv);
  }
}


void
chatty_lurch_fp_device_get (PurpleConversation *conv)
{
  PurpleAccount * account;

  account = purple_conversation_get_account (conv);

  purple_signal_emit (purple_plugins_get_handle(),
                      "lurch-fp-get",
                      account,
                      cb_get_fingerprint,
                      conv);
}


void
chatty_lurch_enable (PurpleConversation *conv)
{
  PurpleAccount          *account;
  PurpleConversationType  type;
  const char             *name;

  account = purple_conversation_get_account (conv);
  type = purple_conversation_get_type (conv);
  name = purple_conversation_get_name (conv);

  if (type == PURPLE_CONV_TYPE_IM) {
    purple_signal_emit (purple_plugins_get_handle(),
                        "lurch-enable-im",
                        account,
                        chatty_utils_jabber_id_strip (name),
                        cb_set_enable,
                        conv);
  }
}


void
chatty_lurch_disable (PurpleConversation *conv)
{
  PurpleAccount          *account;
  PurpleConversationType  type;
  const char             *name;

  account = purple_conversation_get_account (conv);
  type = purple_conversation_get_type (conv);
  name = purple_conversation_get_name (conv);

  if (type == PURPLE_CONV_TYPE_IM) {
    purple_signal_emit (purple_plugins_get_handle(),
                        "lurch-disable-im",
                        account,
                        chatty_utils_jabber_id_strip (name),
                        cb_set_disable,
                        conv);
  }
}


void
chatty_lurch_get_status (PurpleConversation *conv)
{
  PurpleAccount          *account;
  PurpleConversationType  type;
  const char             *name;

  account = purple_conversation_get_account (conv);
  type = purple_conversation_get_type (conv);
  name = purple_conversation_get_name (conv);

  if (type == PURPLE_CONV_TYPE_IM) {
    purple_signal_emit (purple_plugins_get_handle(),
                        "lurch-status-im",
                        account,
                        chatty_utils_jabber_id_strip (name),
                        cb_get_status,
                        conv);
  }
}
