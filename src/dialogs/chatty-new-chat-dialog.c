/*
 * Copyright (C) 2020 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */



#define G_LOG_DOMAIN "chatty-new-chat-dialog"

#define _GNU_SOURCE
#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include "chatty-window.h"
#include "chatty-manager.h"
#include "chatty-conversation.h"
#include "chatty-chat.h"
#include "users/chatty-contact.h"
#include "contrib/gtk.h"
#include "users/chatty-pp-account.h"
#include "chatty-buddy-list.h"
#include "chatty-list-row.h"
#include "chatty-dbus.h"
#include "chatty-utils.h"
#include "chatty-folks.h"
#include "chatty-icons.h"
#include "chatty-new-chat-dialog.h"


static void chatty_new_chat_dialog_update (ChattyNewChatDialog *self);

static void chatty_new_chat_name_check (ChattyNewChatDialog *self, 
                                        GtkEntry            *entry, 
                                        GtkWidget           *button);


#define ITEMS_COUNT 50

struct _ChattyNewChatDialog
{
  HdyDialog  parent_instance;

  GtkWidget *chats_listbox;
  GtkWidget *new_contact_row;
  GtkWidget *contacts_search_entry;
  GtkWidget *contact_edit_grid;
  GtkWidget *new_chat_stack;
  GtkWidget *accounts_list;
  GtkWidget *contact_name_entry;
  GtkWidget *contact_alias_entry;
  GtkWidget *back_button;
  GtkWidget *add_contact_button;
  GtkWidget *edit_contact_button;
  GtkWidget *add_in_contacts_button;
  GtkWidget *dummy_prefix_radio;

  GtkSliceListModel  *slice_model;
  GtkFilterListModel *filter_model;
  GtkFilter *filter;
  char      *search_str;

  ChattyPpAccount *selected_account;
  ChattyManager   *manager;
  ChattyProtocol   active_protocols;
};


G_DEFINE_TYPE (ChattyNewChatDialog, chatty_new_chat_dialog, HDY_TYPE_DIALOG)


static void
dialog_active_protocols_changed_cb (ChattyNewChatDialog *self)
{
  g_assert (CHATTY_IS_NEW_CHAT_DIALOG (self));

  self->active_protocols = chatty_manager_get_active_protocols (self->manager);
  gtk_filter_changed (self->filter, GTK_FILTER_CHANGE_DIFFERENT);
}


static gboolean
dialog_filter_item_cb (ChattyItem          *item,
                       ChattyNewChatDialog *self)
{
  g_return_val_if_fail (CHATTY_IS_NEW_CHAT_DIALOG (self), FALSE);

  if (CHATTY_IS_PP_BUDDY (item))
    if (chatty_pp_buddy_get_contact (CHATTY_PP_BUDDY (item)))
      return FALSE;

  return chatty_item_matches (item, self->search_str, self->active_protocols, TRUE);
}


static void
chatty_new_chat_dialog_update_new_contact_row (ChattyNewChatDialog *self)
{
  GdkPixbuf *avatar;

  g_assert (CHATTY_IS_NEW_CHAT_DIALOG (self));

  avatar = chatty_icon_get_buddy_icon (NULL,
                                       "+",
                                       CHATTY_ICON_SIZE_MEDIUM,
                                       CHATTY_COLOR_GREY,
                                       FALSE);
  g_object_set (self->new_contact_row, "avatar", avatar, NULL);
}

static void
chatty_group_chat_action (GSimpleAction *action,
                          GVariant      *parameter,
                          gpointer       user_data)
{
  ChattyWindow *window = user_data;

  chatty_window_change_view (window, CHATTY_VIEW_JOIN_CHAT);
}


static void
chatty_direct_chat_action (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
  ChattyWindow *window = user_data;

  chatty_window_change_view (window, CHATTY_VIEW_NEW_CHAT);
}


static void
chatty_settings_action (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{
  ChattyWindow *window = user_data;

  chatty_window_change_view (window, CHATTY_VIEW_SETTINGS);
}


static void
chatty_about_Action (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  ChattyWindow *window = user_data;

  chatty_window_change_view (window, CHATTY_VIEW_ABOUT_CHATTY);
}


static GtkWidget *
dialog_create_chat_row (ChattyChat *chat)
{
  GdkPixbuf     *avatar;
  PurpleChat    *pp_chat;
  const gchar   *chat_name;
  const gchar   *account_name;

  g_assert (CHATTY_IS_CHAT (chat));

  pp_chat = chatty_chat_get_purple_chat (chat);

  avatar = chatty_icon_get_buddy_icon ((PurpleBlistNode *)pp_chat,
                                       NULL,
                                       CHATTY_ICON_SIZE_MEDIUM,
                                       CHATTY_COLOR_BLUE,
                                       FALSE);
  account_name = purple_account_get_username (pp_chat->account);
  chat_name = purple_chat_get_name (pp_chat);
  return chatty_contact_row_new ((gpointer) pp_chat,
                                 avatar,
                                 chat_name,
                                 account_name,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 FALSE);
}

static GtkWidget *
dialog_contact_row_new (GObject *object)
{
  if (CHATTY_IS_CONTACT (object) ||
      CHATTY_IS_PP_BUDDY (object))
    return chatty_list_row_new (CHATTY_ITEM (object));
  else
    return dialog_create_chat_row (CHATTY_CHAT (object));
}


static void
back_button_clicked_cb (ChattyNewChatDialog *self)
{
  g_assert (CHATTY_IS_NEW_CHAT_DIALOG (self));

  gtk_stack_set_visible_child_name (GTK_STACK (self->new_chat_stack), "view-new-chat");
}


static void
edit_contact_button_clicked_cb (ChattyNewChatDialog *self)
{
  g_assert (CHATTY_IS_NEW_CHAT_DIALOG (self));

  chatty_new_chat_dialog_update (self);

  gtk_stack_set_visible_child_name (GTK_STACK (self->new_chat_stack), "view-new-contact");
}


static void
add_in_contacts_button_clicked_cb (ChattyNewChatDialog *self)
{
  g_assert (CHATTY_IS_NEW_CHAT_DIALOG (self));

  chatty_dbus_gc_write_contact ("", "");
   
  gtk_stack_set_visible_child_name (GTK_STACK (self->new_chat_stack), "view-new-chat");
}



static void
contact_stroll_edge_reached_cb (ChattyNewChatDialog *self,
                                GtkPositionType      position)
{
  const char *name;

  g_assert (CHATTY_IS_NEW_CHAT_DIALOG (self));

  if (position != GTK_POS_BOTTOM)
    return;

  name = gtk_stack_get_visible_child_name (GTK_STACK (self->new_chat_stack));

  if (!g_str_equal (name, "view-new-chat"))
    return;

  gtk_slice_list_model_set_size (self->slice_model,
                                 gtk_slice_list_model_get_size (self->slice_model) + ITEMS_COUNT);
}

static void
contact_search_entry_changed_cb (ChattyNewChatDialog *self,
                                 GtkEntry            *entry)
{
  g_autofree char *old_needle = NULL;
  const char *str;
  GtkFilterChange change;
  guint n_items;

  g_assert (CHATTY_IS_NEW_CHAT_DIALOG (self));
  g_assert (GTK_IS_ENTRY (entry));

  str = gtk_entry_get_text (entry);

  if (!str)
    str = "";

  old_needle = self->search_str;
  self->search_str = g_utf8_casefold (str, -1);

  if (!old_needle)
    old_needle = g_strdup ("");

  if (g_str_has_prefix (self->search_str, old_needle))
    change = GTK_FILTER_CHANGE_MORE_STRICT;
  else if (g_str_has_prefix (old_needle, self->search_str))
    change = GTK_FILTER_CHANGE_LESS_STRICT;
  else
    change = GTK_FILTER_CHANGE_DIFFERENT;

  gtk_slice_list_model_set_size (self->slice_model, ITEMS_COUNT);
  gtk_filter_changed (self->filter, change);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->filter_model));

  if (n_items == 0) {
    char *number;

    number = chatty_utils_check_phonenumber (self->search_str);

    gtk_widget_set_visible (self->chats_listbox, number == NULL);

    if (number)
      g_object_set (self->new_contact_row,
                    "description", number,
                    "phone-number", number,
                    "message-count", NULL,
                    NULL);
  } else {
    gtk_widget_show (self->chats_listbox);
  }
}

static void
contact_row_activated_cb (ChattyNewChatDialog *self,
                          GtkListBoxRow       *row)
{
  ChattyWindow    *window;
  PurpleBlistNode *node;
  PurpleAccount   *account;
  PurpleChat      *chat;
  GdkPixbuf       *avatar;
  const char      *chat_name;
  const char      *number;

  g_assert (CHATTY_IS_NEW_CHAT_DIALOG (self));

  if (CHATTY_IS_CONTACT_ROW (row)) {
    g_object_get (row, "data", &node, NULL);
    g_object_get (row, "phone_number", &number, NULL);

    if (number != NULL) {
      chatty_blist_add_buddy_from_uri (number);

      return;
    }
  } else if (CHATTY_IS_LIST_ROW (row)) {
    ChattyItem *item;

    item = chatty_list_row_get_item (CHATTY_LIST_ROW (row));

    if (CHATTY_IS_CONTACT (item)) {
      number = chatty_contact_get_value (CHATTY_CONTACT (item));
      chatty_blist_add_buddy_from_uri (number);

      return;
    }

    node = (PurpleBlistNode *)chatty_pp_buddy_get_buddy (CHATTY_PP_BUDDY (item));
  } else {
    g_return_if_reached ();
  }

  window = chatty_utils_get_window ();

  chatty_window_set_menu_add_contact_button_visible (window, FALSE);
  chatty_window_set_menu_add_in_contacts_button_visible (window, FALSE);

  purple_blist_node_set_bool (node, "chatty-autojoin", TRUE);
  purple_blist_node_set_bool (node, "chatty-notifications", TRUE);

  if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
    PurpleBuddy *buddy;

    buddy = (PurpleBuddy*)node;
    account = purple_buddy_get_account (buddy);

    chatty_window_set_header_chat_info_button_visible (window, FALSE);

    if (chatty_blist_protocol_is_sms (account)) {
      ChattyEds *chatty_eds;
      ChattyContact *contact;

      chatty_eds = chatty_manager_get_eds (chatty_manager_get_default ());
      number = purple_buddy_get_name (buddy);
      contact = chatty_eds_find_by_number (chatty_eds, number);

      if (!contact) {
        chatty_window_set_menu_add_in_contacts_button_visible (window, TRUE);
      }
    }

    if (purple_blist_node_get_bool (PURPLE_BLIST_NODE(buddy),
                                    "chatty-unknown-contact")) {

      chatty_window_set_menu_add_contact_button_visible (window, TRUE);
    }

    chatty_conv_im_with_buddy (account, purple_buddy_get_name (buddy));

    chatty_window_set_new_chat_dialog_visible (window, FALSE);

  } else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
    chat = (PurpleChat*)node;
    chat_name = purple_chat_get_name (chat);

    chatty_conv_join_chat (chat);

    avatar = chatty_icon_get_buddy_icon (node,
                                         NULL,
                                         CHATTY_ICON_SIZE_SMALL,
                                         CHATTY_COLOR_GREY,
                                         FALSE);

    chatty_window_update_sub_header_titlebar (window, avatar, chat_name);
    chatty_window_change_view (window, CHATTY_VIEW_MESSAGE_LIST);

    chatty_window_set_new_chat_dialog_visible (window, FALSE);

    g_object_unref (avatar);
  }
}

static void
add_contact_button_clicked_cb (ChattyNewChatDialog *self)
{
  PurpleAccount     *account;
  char              *who;
  const char        *alias;
  g_autoptr(GError)  err = NULL;

  g_assert (CHATTY_IS_NEW_CHAT_DIALOG (self));

  account = chatty_pp_account_get_account (self->selected_account);

  who = g_strdup (gtk_entry_get_text (GTK_ENTRY (self->contact_name_entry)));
  alias = gtk_entry_get_text (GTK_ENTRY (self->contact_alias_entry));

  chatty_pp_account_add_buddy (self->selected_account, who, alias);
  chatty_conv_im_with_buddy (account, g_strdup (who));

  gtk_widget_hide (GTK_WIDGET (self));

  g_free (who);

  gtk_entry_set_text (GTK_ENTRY (self->contact_name_entry), "");
  gtk_entry_set_text (GTK_ENTRY (self->contact_alias_entry), "");

  gtk_stack_set_visible_child_name (GTK_STACK (self->new_chat_stack), "view-new-chat");
}


static void
contact_name_text_changed_cb (ChattyNewChatDialog *self)
{
  g_assert (CHATTY_IS_NEW_CHAT_DIALOG (self));

  chatty_new_chat_name_check (self,
                              GTK_ENTRY (self->contact_name_entry), 
                              self->add_contact_button);
}


static void
account_list_row_activated_cb (ChattyNewChatDialog *self,
                               GtkListBoxRow       *row,
                               GtkListBox          *box)
{
  ChattyPpAccount *account;
  GtkWidget       *prefix_radio;

  g_assert (CHATTY_IS_NEW_CHAT_DIALOG (self));

  account = g_object_get_data (G_OBJECT (row), "row-account");
  prefix_radio = g_object_get_data (G_OBJECT (row), "row-prefix");

  self->selected_account = account;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefix_radio), TRUE);

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
  PurpleBuddy   *buddy = NULL;
  const char    *name;

  g_return_if_fail (CHATTY_IS_NEW_CHAT_DIALOG (self));

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
  g_return_if_fail (CHATTY_IS_NEW_CHAT_DIALOG (self));
  
  if (edit) {
    gtk_widget_show (GTK_WIDGET (self->contact_edit_grid));
    gtk_widget_show (GTK_WIDGET (self->add_contact_button));
    gtk_widget_hide (GTK_WIDGET (self->add_in_contacts_button));
  } else {
    gtk_widget_hide (GTK_WIDGET (self->contact_edit_grid));
    gtk_widget_hide (GTK_WIDGET (self->add_contact_button));
    gtk_widget_show (GTK_WIDGET (self->add_in_contacts_button));
  }
}


static void
chatty_new_chat_add_account_to_list (ChattyNewChatDialog *self,
                                     ChattyPpAccount     *account)
{
  HdyActionRow   *row;
  GtkWidget      *prefix_radio_button;
  ChattyProtocol  protocol;

  g_return_if_fail (CHATTY_IS_NEW_CHAT_DIALOG (self));

  row = hdy_action_row_new ();
  g_object_set_data (G_OBJECT (row),
                     "row-account",
                     (gpointer)account);

  protocol = chatty_item_get_protocols (CHATTY_ITEM (account));

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

  prefix_radio_button = gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON (self->dummy_prefix_radio));
  gtk_widget_show (GTK_WIDGET (prefix_radio_button));

  gtk_widget_set_sensitive (prefix_radio_button, FALSE);

  g_object_set_data (G_OBJECT (row),
                     "row-prefix",
                     (gpointer)prefix_radio_button);

  hdy_action_row_add_prefix (row, GTK_WIDGET (prefix_radio_button ));
  hdy_action_row_set_title (row, chatty_pp_account_get_username (account));

  gtk_container_add (GTK_CONTAINER (self->accounts_list), GTK_WIDGET (row));

  gtk_widget_show (GTK_WIDGET (row));
}


static void
chatty_new_chat_account_list_clear (GtkWidget *list)
{
  GList *children;
  GList *iter;

  g_return_if_fail (GTK_IS_LIST_BOX(list));

  children = gtk_container_get_children (GTK_CONTAINER (list));

  for (iter = children; iter != NULL; iter = g_list_next (iter)) {
    gtk_container_remove (GTK_CONTAINER (list), GTK_WIDGET (iter->data));
  }

  g_list_free (children);
}


static void
chatty_new_chat_populate_account_list (ChattyNewChatDialog *self)
{
  GListModel   *model;
  HdyActionRow *row;
  guint         n_items;

  g_return_if_fail (CHATTY_IS_NEW_CHAT_DIALOG (self));

  chatty_new_chat_account_list_clear (self->accounts_list);

  model = chatty_manager_get_accounts (chatty_manager_get_default ());

  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++) {
    g_autoptr(ChattyPpAccount) account = NULL;

    account = g_list_model_get_item (model, i);

    chatty_new_chat_add_account_to_list (self, account);
  }

  row = HDY_ACTION_ROW(gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->accounts_list), 0));

  if (row) {
    account_list_row_activated_cb (self,
                                   GTK_LIST_BOX_ROW (row), 
                                   GTK_LIST_BOX (self->accounts_list));
  }
}


static void
chatty_new_chat_dialog_update (ChattyNewChatDialog *self)
{
  g_return_if_fail (CHATTY_IS_NEW_CHAT_DIALOG (self));

  gtk_entry_set_text (GTK_ENTRY (self->contact_name_entry), "");
  gtk_entry_set_text (GTK_ENTRY (self->contact_alias_entry), "");

  chatty_new_chat_populate_account_list (self);
}


static void
chatty_new_chat_dialog_constructed (GObject *object)
{
  GtkWindow          *window;
  GSimpleActionGroup *simple_action_group;

  const GActionEntry view_chat_list_entries [] =
  {
    { "group-chat", chatty_group_chat_action },
    { "direct-chat", chatty_direct_chat_action },
    { "settings", chatty_settings_action },
    { "about", chatty_about_Action }
  };

  ChattyNewChatDialog *self = (ChattyNewChatDialog *)object;

  window = gtk_window_get_transient_for (GTK_WINDOW (self));

  simple_action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   view_chat_list_entries,
                                   G_N_ELEMENTS (view_chat_list_entries),
                                   window);

  gtk_widget_insert_action_group (GTK_WIDGET (window),
                                  "chat_list",
                                  G_ACTION_GROUP (simple_action_group));
}


static void
chatty_new_chat_dialog_finalize (GObject *object)
{
  ChattyNewChatDialog *self = (ChattyNewChatDialog *)object;

  g_clear_object (&self->manager);

  G_OBJECT_CLASS (chatty_new_chat_dialog_parent_class)->finalize (object);
}

static void
chatty_new_chat_dialog_class_init (ChattyNewChatDialogClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed  = chatty_new_chat_dialog_constructed;
  object_class->finalize = chatty_new_chat_dialog_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/chatty/"
                                               "ui/chatty-dialog-new-chat.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, new_chat_stack);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, contacts_search_entry);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, new_contact_row);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, chats_listbox);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, contact_edit_grid);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, contact_name_entry);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, contact_alias_entry);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, accounts_list);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, back_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, edit_contact_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, add_contact_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, add_in_contacts_button);

  gtk_widget_class_bind_template_callback (widget_class, back_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, edit_contact_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, contact_stroll_edge_reached_cb);
  gtk_widget_class_bind_template_callback (widget_class, contact_search_entry_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, contact_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, add_contact_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, add_in_contacts_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, contact_name_text_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, account_list_row_activated_cb);
}


static void
chatty_new_chat_dialog_init (ChattyNewChatDialog *self)
{
  g_autoptr(GtkSortListModel) sort_model = NULL;
  g_autoptr(GtkSorter) sorter = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->accounts_list),
                                hdy_list_box_separator_header,
                                NULL, NULL);

  self->dummy_prefix_radio = gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON (NULL));

  self->manager = g_object_ref (chatty_manager_get_default ());
  self->filter = gtk_custom_filter_new ((GtkCustomFilterFunc)dialog_filter_item_cb,
                                        self, NULL);
  g_signal_connect_object (self->manager, "notify::active-protocols",
                           G_CALLBACK (dialog_active_protocols_changed_cb), self, G_CONNECT_SWAPPED);
  dialog_active_protocols_changed_cb (self);

  sorter = gtk_custom_sorter_new ((GCompareDataFunc)chatty_item_compare, NULL, NULL);
  sort_model = gtk_sort_list_model_new (chatty_manager_get_contact_list (self->manager), sorter);
  self->filter_model = gtk_filter_list_model_new (G_LIST_MODEL (sort_model), self->filter);
  self->slice_model = gtk_slice_list_model_new (G_LIST_MODEL (self->filter_model), 0, ITEMS_COUNT);
  gtk_list_box_bind_model (GTK_LIST_BOX (self->chats_listbox),
                           G_LIST_MODEL (self->slice_model),
                           (GtkListBoxCreateWidgetFunc)dialog_contact_row_new,
                           NULL, NULL);

  chatty_new_chat_dialog_update_new_contact_row (self);
}


GtkWidget *
chatty_new_chat_dialog_new (GtkWindow *parent_window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (parent_window), NULL);

  return g_object_new (CHATTY_TYPE_NEW_CHAT_DIALOG,
                       "transient-for", parent_window,
                       "use-header-bar", 1,
                       NULL);
}
