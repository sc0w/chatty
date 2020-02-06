/*
 * Copyright (C) 2020 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */



#define G_LOG_DOMAIN "chatty-new-chat-dialog"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include "chatty-window.h"
#include "chatty-manager.h"
#include "chatty-dialogs.h"
#include "users/chatty-pp-account.h"
#include "chatty-buddy-list.h"
#include "chatty-dbus.h"
#include "chatty-utils.h"
#include "chatty-new-chat-dialog.h"


static void chatty_new_chat_dialog_update (ChattyNewChatDialog *self);

static void chatty_new_chat_name_check (ChattyNewChatDialog *self, 
                                        GtkEntry            *entry, 
                                        GtkWidget           *button);


struct _ChattyNewChatDialog
{
  HdyDialog  parent_instance;

  GtkWidget *list_box_chats;
  GtkWidget *search_entry_contacts;
  GtkWidget *grid_edit_contact;
  GtkWidget *button_add_gnome_contact;
  GtkWidget *stack_panes_new_chat;
  GtkWidget *list_select_account;
  GtkWidget *entry_contact_name;
  GtkWidget *entry_contact_alias;
  GtkWidget *button_add_contact;
  GtkWidget *button_back;
  GtkWidget *button_show_add_contact;
  GtkWidget *dummy_prefix_radio;

  ChattyPpAccount *selected_account;
};


G_DEFINE_TYPE (ChattyNewChatDialog, chatty_new_chat_dialog, HDY_TYPE_DIALOG)


static void
button_new_chat_back_clicked_cb (ChattyNewChatDialog *self)
{
  g_assert (CHATTY_IS_NEW_CHAT_DIALOG(self));

  gtk_stack_set_visible_child_name (GTK_STACK(self->stack_panes_new_chat),
                                    "view-new-chat");
}


static void
button_show_add_contact_clicked_cb (ChattyNewChatDialog *self)
{
  g_assert (CHATTY_IS_NEW_CHAT_DIALOG(self));

  chatty_new_chat_dialog_update (self);

  gtk_stack_set_visible_child_name (GTK_STACK(self->stack_panes_new_chat),
                                    "view-new-contact");
}


static void
button_add_gnome_contact_clicked_cb (ChattyNewChatDialog *self)
{
  g_assert (CHATTY_IS_NEW_CHAT_DIALOG(self));

  chatty_dbus_gc_write_contact ("", "");
   
  gtk_stack_set_visible_child_name (GTK_STACK(self->stack_panes_new_chat),
                                    "view-new-chat");
}


static void
button_add_contact_clicked_cb (ChattyNewChatDialog *self)
{
  PurpleAccount     *account;
  char              *who;
  const char        *alias;
  g_autoptr(GError)  err = NULL;

  g_assert (CHATTY_IS_NEW_CHAT_DIALOG(self));

  account = chatty_pp_account_get_account (self->selected_account);

  who = g_strdup (gtk_entry_get_text (GTK_ENTRY(self->entry_contact_name)));
  alias = gtk_entry_get_text (GTK_ENTRY(self->entry_contact_alias));

  chatty_pp_account_add_buddy (self->selected_account, who, alias);
  chatty_conv_im_with_buddy (account, g_strdup (who));

  gtk_widget_hide (GTK_WIDGET(self));

  g_free (who);

  gtk_entry_set_text (GTK_ENTRY(self->entry_contact_name), "");
  gtk_entry_set_text (GTK_ENTRY(self->entry_contact_alias), "");

  gtk_stack_set_visible_child_name (GTK_STACK(self->stack_panes_new_chat),
                                    "view-new-chat");
}


static void
contact_name_text_changed_cb (ChattyNewChatDialog *self)
{
  g_assert (CHATTY_IS_NEW_CHAT_DIALOG(self));

  chatty_new_chat_name_check (self,
                              GTK_ENTRY(self->entry_contact_name), 
                              self->button_add_contact);
}


static void
account_list_row_activated_cb (ChattyNewChatDialog *self,
                               GtkListBoxRow       *row,
                               GtkListBox          *box)
{
  ChattyPpAccount *account;
  GtkWidget       *prefix_radio;

  g_assert (CHATTY_IS_NEW_CHAT_DIALOG(self));

  account = g_object_get_data (G_OBJECT(row), "row-account");
  prefix_radio = g_object_get_data (G_OBJECT(row), "row-prefix");

  self->selected_account = account;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(prefix_radio), TRUE);

  if (chatty_pp_account_is_sms (account)) {
    chatty_new_chat_set_edit_mode (self, FALSE);
  } else {
    chatty_new_chat_set_edit_mode (self, TRUE);
  }
}


static void
chatty_new_chat_name_check (ChattyNewChatDialog *self,
                            GtkEntry            *entry,
                            GtkWidget           *button)
{
  PurpleAccount *account;
  PurpleBuddy   *buddy;
  const char    *name;

  g_return_if_fail (CHATTY_IS_NEW_CHAT_DIALOG(self));

  name = gtk_entry_get_text (entry);

  account = chatty_pp_account_get_account (self->selected_account);

  if ((*name != '\0') && account) {
    buddy = purple_find_buddy (account, name);
  }

  if ((*name != '\0') && !buddy) {
    gtk_widget_set_sensitive (button, TRUE);
  } else {
    gtk_widget_set_sensitive (button, FALSE);
  }
}


void
chatty_new_chat_set_edit_mode (ChattyNewChatDialog *self,
                               gboolean             edit)
{
  g_return_if_fail (CHATTY_IS_NEW_CHAT_DIALOG(self));
  
  if (edit) {
    gtk_widget_show (GTK_WIDGET(self->grid_edit_contact));
    gtk_widget_show (GTK_WIDGET(self->button_add_contact));
    gtk_widget_hide (GTK_WIDGET(self->button_add_gnome_contact));
  } else {
    gtk_widget_hide (GTK_WIDGET(self->grid_edit_contact));
    gtk_widget_hide (GTK_WIDGET(self->button_add_contact));
    gtk_widget_show (GTK_WIDGET(self->button_add_gnome_contact));
  }
}


static void
chatty_new_chat_add_account_to_list (ChattyNewChatDialog *self,
                                     ChattyPpAccount     *account)
{
  HdyActionRow *row;
  GtkWidget    *prefix_radio_button;
  ChattyProtocol protocol;

  g_return_if_fail (CHATTY_IS_NEW_CHAT_DIALOG(self));

  row = hdy_action_row_new ();
  g_object_set_data (G_OBJECT(row),
                     "row-account",
                     (gpointer) account);

  protocol = chatty_user_get_protocols (CHATTY_USER (account));

  // TODO list supported protocols here
  if (protocol & ~(CHATTY_PROTOCOL_SMS |
                   CHATTY_PROTOCOL_XMPP |
                   CHATTY_PROTOCOL_MATRIX |
                   CHATTY_PROTOCOL_TELEGRAM |
                   CHATTY_PROTOCOL_DELTA |
                   CHATTY_PROTOCOL_THREEPL))
    return;

  if (chatty_account_get_status (CHATTY_ACCOUNT (account)) == CHATTY_DISCONNECTED) {
    return;
  }

  prefix_radio_button = gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON(self->dummy_prefix_radio));
  gtk_widget_show (GTK_WIDGET(prefix_radio_button));

  gtk_widget_set_sensitive (prefix_radio_button, FALSE);

  g_object_set_data (G_OBJECT(row),
                     "row-prefix",
                     (gpointer)prefix_radio_button);

  hdy_action_row_add_prefix (row, GTK_WIDGET(prefix_radio_button ));
  hdy_action_row_set_title (row, chatty_pp_account_get_username (account));

  gtk_container_add (GTK_CONTAINER(self->list_select_account), GTK_WIDGET(row));

  gtk_widget_show (GTK_WIDGET(row));
}


static void
chatty_new_chat_account_list_clear (GtkWidget *list)
{
  GList *children;
  GList *iter;

  g_return_if_fail (GTK_IS_LIST_BOX(list));

  children = gtk_container_get_children (GTK_CONTAINER(list));

  for (iter = children; iter != NULL; iter = g_list_next (iter)) {
    gtk_container_remove (GTK_CONTAINER(list), GTK_WIDGET(iter->data));
  }

  g_list_free (children);
}


static void
chatty_new_chat_populate_account_list (ChattyNewChatDialog *self)
{
  GListModel *model;
  HdyActionRow  *row;
  guint n_items;

  g_return_if_fail (CHATTY_IS_NEW_CHAT_DIALOG (self));

  chatty_new_chat_account_list_clear (self->list_select_account);

  model = chatty_manager_get_accounts (chatty_manager_get_default ());
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(ChattyPpAccount) account = NULL;

      account = g_list_model_get_item (model, i);
      chatty_new_chat_add_account_to_list (self, account);
    }

  row = HDY_ACTION_ROW(gtk_list_box_get_row_at_index (GTK_LIST_BOX(self->list_select_account), 0));

  if (row) {
    account_list_row_activated_cb (self,
                                   GTK_LIST_BOX_ROW(row), 
                                   GTK_LIST_BOX(self->list_select_account));
  }
}


static void
chatty_new_chat_dialog_update (ChattyNewChatDialog *self)
{
  g_return_if_fail (CHATTY_IS_NEW_CHAT_DIALOG(self));

  gtk_entry_set_text (GTK_ENTRY(self->entry_contact_name), "");
  gtk_entry_set_text (GTK_ENTRY(self->entry_contact_alias), "");

  chatty_new_chat_populate_account_list (self);
}


GtkWidget *
chatty_new_chat_get_list_contacts (ChattyNewChatDialog *self)
{
  g_return_val_if_fail (CHATTY_IS_NEW_CHAT_DIALOG(self), NULL);
  
  return self->list_box_chats;
}


GtkWidget *
chatty_new_chat_get_search_entry (ChattyNewChatDialog *self)
{
  g_return_val_if_fail (CHATTY_IS_NEW_CHAT_DIALOG(self), NULL);

  return self->search_entry_contacts;
}


static void
chatty_new_chat_dialog_class_init (ChattyNewChatDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/chatty/"
                                               "ui/chatty-dialog-new-chat.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, list_box_chats);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, search_entry_contacts);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, grid_edit_contact);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, button_add_gnome_contact);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, stack_panes_new_chat);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, list_select_account);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, entry_contact_name);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, entry_contact_alias);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, button_add_contact);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, button_back);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, button_show_add_contact);

  gtk_widget_class_bind_template_callback (widget_class, button_new_chat_back_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, button_show_add_contact_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, button_add_contact_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, button_add_gnome_contact_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, contact_name_text_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, account_list_row_activated_cb);
}


static void
chatty_new_chat_dialog_init (ChattyNewChatDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET(self));

  gtk_list_box_set_header_func (GTK_LIST_BOX(self->list_select_account),
                                hdy_list_box_separator_header,
                                NULL, NULL);

  self->dummy_prefix_radio = gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON(NULL));
}


GtkWidget *
chatty_new_chat_dialog_new (GtkWindow *parent_window)
{
  g_return_val_if_fail (GTK_IS_WINDOW(parent_window), NULL);

  return g_object_new (CHATTY_TYPE_NEW_CHAT_DIALOG,
                       "transient-for", parent_window,
                       "use-header-bar", 1,
                       NULL);
}
