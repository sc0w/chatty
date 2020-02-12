/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-buddy-list"

#define _GNU_SOURCE
#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <cairo.h>
#include "purple.h"
#include "chatty-icons.h"
#include "chatty-window.h"
#include "chatty-purple-init.h"
#include "chatty-contact-row.h"
#include "chatty-buddy-list.h"
#include "chatty-conversation.h"
#include "chatty-history.h"
#include "chatty-utils.h"
#include "chatty-folks.h"
#include "chatty-dbus.h"
#define HANDY_USE_UNSTABLE_API
#include <handy.h>
#include "dialogs/chatty-new-chat-dialog.h"


static void chatty_blist_new_node (PurpleBlistNode *node);

static void chatty_blist_update (PurpleBuddyList *list,
                                 PurpleBlistNode *node);

static void chatty_blist_chats_remove_node (PurpleBlistNode *node);

static void chatty_blist_contacts_remove_node (PurpleBlistNode *node);

static void chatty_blist_update_buddy (PurpleBuddyList *list,
                                       PurpleBlistNode *node);

static gint chatty_blist_sort (GtkListBoxRow *row1, GtkListBoxRow *row2, gpointer user_data);

static GtkListBox *
chatty_get_contacts_list (void) 
{
  ChattyWindow  *window;
  GtkWidget     *dialog;
  GtkListBox    *list;

  window = chatty_utils_get_window ();

  dialog = chatty_window_get_new_chat_dialog (window);

  list = GTK_LIST_BOX(chatty_new_chat_get_list_contacts ((ChattyNewChatDialog*)dialog));

  return list;
}

static GtkListBox *
chatty_get_chats_list (void) 
{
  GtkListBox   *list;
  ChattyWindow *window;

  window = chatty_utils_get_window ();

  list = GTK_LIST_BOX(chatty_window_get_chats_listbox (window));

  return list;
}

static int list_refresh_timer;

// *** callbacks
static void
row_selected_cb (GtkListBox    *box,
                 GtkListBoxRow *row,
                 gpointer       user_data)
{
  ChattyWindow    *window;
  PurpleBlistNode *node;
  PurpleAccount   *account;
  PurpleChat      *chat;
  GdkPixbuf       *avatar;
  const char      *chat_name;
  const char      *number;
  const char      *folks_id;  


  if (row == NULL) {
    return;
  }

  g_object_get (row, "phone_number", &number, NULL);

  if (number != NULL) {
    chatty_blist_add_buddy_from_uri (number);

    return;
  }

  window = chatty_utils_get_window ();

  g_object_get (row, "data", &node, NULL);

  chatty_window_set_menu_add_contact_button_visible (window, FALSE);
  chatty_window_set_menu_add_in_contacts_button_visible (window, FALSE);

  if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
    PurpleBuddy *buddy;

    buddy = (PurpleBuddy*)node;
    account = purple_buddy_get_account (buddy);

    chatty_window_set_header_chat_info_button_visible (window, FALSE);

    if (chatty_blist_protocol_is_sms (account)) {
      number = purple_buddy_get_name (buddy);
      folks_id = chatty_folks_has_individual_with_phonenumber (number);

      if (!folks_id) {
        chatty_window_set_menu_add_in_contacts_button_visible (window, TRUE);
      }
    }

    if (purple_blist_node_get_bool (PURPLE_BLIST_NODE(buddy),
                                    "chatty-unknown-contact")) {

      chatty_window_set_menu_add_contact_button_visible (window, TRUE);
    }

    purple_blist_node_set_bool (node, "chatty-autojoin", TRUE);

    chatty_conv_im_with_buddy (account, purple_buddy_get_name (buddy));

    chatty_window_set_new_chat_dialog_visible (window, FALSE);

  } else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
    chat = (PurpleChat*)node;
    chat_name = purple_chat_get_name (chat);

    chatty_conv_join_chat (chat);

    purple_blist_node_set_bool (node, "chatty-autojoin", TRUE);

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
cb_chats_search_entry_changed (GtkSearchEntry *entry,
                               GtkListBox     *listbox)
{
  gtk_list_box_invalidate_filter (GTK_LIST_BOX (listbox));
}


static void
cb_search_entry_contacts_changed (GtkSearchEntry *entry,
                                  GtkListBox     *listbox)
{
  PurpleAccount           *account;
  GdkPixbuf               *avatar;
  static ChattyContactRow *new_row;
  GList                   *children, *l;
  const gchar             *number;
  gchar                   *number_verif;
  int                      num_rows = 0;

  account = purple_accounts_find ("SMS", "prpl-mm-sms");

  if (purple_account_is_disconnected (account)) {
    gtk_list_box_invalidate_filter (GTK_LIST_BOX (listbox));
    return;
  }

  children = gtk_container_get_children (GTK_CONTAINER(listbox));

  for (l = children; l != NULL; l = g_list_next (l)) {
    if (gtk_widget_get_child_visible (GTK_WIDGET(l->data))) {
      num_rows ++;
    };
  }

  number = gtk_entry_get_text (GTK_ENTRY(entry));

  avatar = chatty_icon_get_buddy_icon (NULL,
                                       "+",
                                       CHATTY_ICON_SIZE_MEDIUM,
                                       CHATTY_COLOR_GREY,
                                       FALSE);

  if ((num_rows == 0) && !new_row) {
    number_verif = chatty_utils_check_phonenumber (number);

    if (!new_row && number_verif) {
      number_verif = chatty_utils_check_phonenumber (number);

      if (number_verif) {
        listbox = chatty_get_contacts_list ();

        new_row = CHATTY_CONTACT_ROW (chatty_contact_row_new (NULL,
                                                              avatar,
                                                              _("Send to"),
                                                              number_verif,
                                                              NULL,
                                                              NULL,
                                                              NULL,
                                                              number_verif,
                                                              FALSE));

        gtk_list_box_row_set_selectable (GTK_LIST_BOX_ROW (new_row), FALSE);

        gtk_container_add (GTK_CONTAINER (listbox), GTK_WIDGET (new_row));

        gtk_widget_show (GTK_WIDGET(new_row));
      }
    }
  } else if (new_row && num_rows == 1 && !(*number == '\0')) {
    number_verif = chatty_utils_check_phonenumber (number);

    if (number_verif) {
      g_object_set (new_row,
                    "description", number,
                    "phone_number", number_verif,
                    NULL);
    }
  } else {
    if (new_row) {
      gtk_widget_destroy (GTK_WIDGET(new_row));
      new_row = NULL;
    }
  }

  if (avatar) {
    g_object_unref (avatar);
  }

  g_list_free (children);

  gtk_list_box_invalidate_filter (GTK_LIST_BOX (listbox));
}


static gboolean
filter_chat_list_cb (GtkListBoxRow *row, gpointer entry) {
  const gchar *query;
  g_autofree gchar *name = NULL;

  query = gtk_entry_get_text (GTK_ENTRY (entry));

  g_object_get (row, "name", &name, NULL);

  return ((*query == '\0') || (name && strcasestr (name, query)));
}


static gboolean
filter_contacts_list_cb (GtkListBoxRow *row, gpointer entry) {
  const gchar *query;
  g_autofree gchar *name = NULL;
  g_autofree gchar *number = NULL;

  query = gtk_entry_get_text (GTK_ENTRY (entry));

  g_object_get (row, "name", &name, NULL);
  g_object_get (row, "description", &number, NULL);

  return ((*query == '\0') || 
          (name && strcasestr (name, query)) ||
          (number && strcasestr (number, query)));
}


static void
cb_buddy_away (PurpleBuddy  *buddy,
               PurpleStatus *old_status,
               PurpleStatus *status)
{
  // TODO set the status in the message list popover
  g_debug ("Buddy \"%s\" (%s) changed status to %s",
            purple_buddy_get_name (buddy),
            purple_account_get_protocol_id (purple_buddy_get_account (buddy)),
            purple_status_get_id (status));
}


static void
cb_buddy_idle (PurpleBuddy *buddy,
               gboolean     old_idle,
               gboolean     idle)
{
  // TODO set the status in the message list popover
  g_debug ("Buddy \"%s\" (%s) changed idle state to %s",
            purple_buddy_get_name(buddy),
            purple_account_get_protocol_id (purple_buddy_get_account (buddy)),
            (idle) ? "idle" : "not idle");
}


static gboolean
cb_buddy_signonoff_timeout (PurpleBuddy *buddy)
{
  ChattyBlistNode *chatty_node = ((PurpleBlistNode*)buddy)->ui_data;

  chatty_node->recent_signonoff_timer = 0;

  chatty_blist_update (purple_get_blist(), (PurpleBlistNode*)buddy);

  return FALSE;
}


static void
cb_chatty_blist_update_privacy (PurpleBuddy *buddy)
{
  struct _chatty_blist_node *ui_data =
    purple_blist_node_get_ui_data (PURPLE_BLIST_NODE(buddy));

  if (ui_data == NULL || ui_data->row_chat == NULL) {
    return;
  }

  chatty_blist_update (purple_get_blist (), PURPLE_BLIST_NODE(buddy));
}


static void
cb_buddy_signed_on_off (PurpleBuddy *buddy)
{
  ChattyBlistNode *chatty_node;

  if (!((PurpleBlistNode*)buddy)->ui_data) {
    chatty_blist_new_node ((PurpleBlistNode*)buddy);
  }

  chatty_node = ((PurpleBlistNode*)buddy)->ui_data;

  if(chatty_node->recent_signonoff_timer > 0) {
    purple_timeout_remove (chatty_node->recent_signonoff_timer);
  }

  chatty_node->recent_signonoff_timer =
    purple_timeout_add_seconds (10,
                                (GSourceFunc)cb_buddy_signonoff_timeout,
                                buddy);

  g_debug ("Buddy \"%s\"\n (%s) signed on/off", purple_buddy_get_name (buddy),
           purple_account_get_protocol_id (purple_buddy_get_account(buddy)));
}


static void
cb_sign_on_off (PurpleConnection  *gc,
                gpointer   *data)
{
  // TODO ...
}


static void
cb_conversation_updated (PurpleConversation   *conv,
                         PurpleConvUpdateType  type,
                         gpointer *data)
{
  GList *convs = NULL;
  GList *l = NULL;

  if (type != PURPLE_CONV_UPDATE_UNSEEN) {
    return;
  }

  if(conv->account != NULL && conv->name != NULL) {
    PurpleBuddy *buddy = purple_find_buddy (conv->account, conv->name);

    if(buddy != NULL) {
      chatty_blist_update (NULL, (PurpleBlistNode *)buddy);
    }
  }

  convs = chatty_conv_find_unseen (CHATTY_UNSEEN_TEXT);

  if (convs) {
    l = convs;

    while (l != NULL) {
      int count = 0;

      ChattyConversation *chatty_conv =
        CHATTY_CONVERSATION((PurpleConversation *)l->data);

      if (chatty_conv) {
        count = chatty_conv->unseen_count;
      } else if (purple_conversation_get_data (l->data, "unseen-count")) {
        count = GPOINTER_TO_INT(purple_conversation_get_data (l->data, "unseen-count"));
      }

      // TODO display the number in a notification icon
      g_debug ("%d unread message from %s",
               count, purple_conversation_get_title (l->data));

      l = l->next;
    }

    g_list_free (convs);
  }
}


static void
cb_conversation_deleting (PurpleConversation  *conv,
                          gpointer data)
{
  cb_conversation_updated (conv, PURPLE_CONV_UPDATE_UNSEEN, data);
}


static void
cb_conversation_deleted_update_ui (PurpleConversation        *conv,
                                   struct _chatty_blist_node *ui)
{
  if (ui->conv.conv != conv) {
    return;
  }

  ui->conv.conv = NULL;
  ui->conv.pending_messages = 0;
}


static void
cb_written_msg_update_ui (PurpleAccount       *account,
                          const char          *who,
                          const char          *message,
                          PurpleConversation  *conv,
                          PurpleMessageFlags   flag,
                          PurpleBlistNode     *node)
{
  ChattyBlistNode *ui = node->ui_data;

  if (ui->conv.conv != conv) {
    return;
  }

  if (flag & (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV) && 
      ui->row_chat != NULL) {
        
    if (!gtk_list_box_row_is_selected (GTK_LIST_BOX_ROW (ui->row_chat))) {
      ui->conv.pending_messages ++;
    }
  }

  chatty_blist_update (purple_get_blist(), node);
}


static void
cb_displayed_msg_update_ui (ChattyConversation *chatty_conv,
                            PurpleBlistNode    *node)
{
  ChattyBlistNode *ui = node->ui_data;

  if (ui->conv.conv != chatty_conv->conv) {
    return;
  }

  ui->conv.pending_messages = 0;

  chatty_blist_update (purple_get_blist(), node);
}


static void
cb_conversation_created (PurpleConversation *conv,
                         gpointer    data)
{
  if (conv->type == PURPLE_CONV_TYPE_IM) {
    GSList *buddies = purple_find_buddies (conv->account, conv->name);

    while (buddies) {
      PurpleBlistNode *buddy = buddies->data;

      struct _chatty_blist_node *ui = buddy->ui_data;

      buddies = g_slist_delete_link (buddies, buddies);

      if (!ui) {
        continue;
      }

      ui->conv.conv = conv;

      purple_signal_connect (purple_conversations_get_handle(),
                             "deleting-conversation",
                             ui,
                             PURPLE_CALLBACK(cb_conversation_deleted_update_ui),
                             ui);

      purple_signal_connect (purple_conversations_get_handle(),
                             "wrote-im-msg",
                             ui,
                             PURPLE_CALLBACK(cb_written_msg_update_ui),
                             buddy);

      purple_signal_connect (chatty_conversations_get_handle(),
                             "conversation-displayed",
                             ui,
                             PURPLE_CALLBACK(cb_displayed_msg_update_ui),
                             buddy);
    }
  }
}


static void
cb_chat_joined (PurpleConversation *conv,
                gpointer    data)
{
  if (conv->type == PURPLE_CONV_TYPE_CHAT) {
    PurpleChat *chat = purple_blist_find_chat(conv->account, conv->name);

    struct _chatty_blist_node *ui;

    if (!chat) {
      return;
    }

    ui = chat->node.ui_data;

    if (!ui) {
      return;
    }

    ui->conv.conv = conv;
    ui->conv.last_message = 0;

    purple_signal_connect (purple_conversations_get_handle(),
                           "deleting-conversation",
                           ui,
                           PURPLE_CALLBACK(cb_conversation_deleted_update_ui),
                           ui);

    purple_signal_connect (purple_conversations_get_handle(),
                           "wrote-chat-msg",
                           ui,
                           PURPLE_CALLBACK(cb_written_msg_update_ui),
                           chat);

    purple_signal_connect (chatty_conversations_get_handle(),
                           "conversation-displayed",
                           ui, PURPLE_CALLBACK(cb_displayed_msg_update_ui),
                           chat);
  }
}


static void
cb_chatty_prefs_changed (void)
{
  chatty_blist_refresh ();
}


static gboolean
cb_auto_join_chats (gpointer data)
{
  PurpleBlistNode  *node;
  PurpleConnection *pc = data;
  GHashTable               *components;
  PurplePluginProtocolInfo *prpl_info;
  PurpleAccount    *account = purple_connection_get_account (pc);

  for (node = purple_blist_get_root (); node;
       node = purple_blist_node_next (node, FALSE)) {

    if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
      PurpleChat *chat = (PurpleChat*)node;

      if (purple_chat_get_account (chat) == account &&
          purple_blist_node_get_bool (node, "chatty-autojoin")) {
        g_autofree char *chat_name;

        prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_find_prpl (purple_account_get_protocol_id (account)));
        components = purple_chat_get_components (chat);
        chat_name = prpl_info->get_chat_name(components);
        chatty_conv_add_history_since_component(components, account->username, chat_name);

        serv_join_chat (purple_account_get_connection (account),
                        purple_chat_get_components (chat));
      }
    }
  }

  return FALSE;
}


static gboolean
cb_chatty_blist_refresh_timer (PurpleBuddyList *list)
{
  cb_chatty_prefs_changed ();

  return TRUE;
}


static gboolean
cb_do_autojoin (PurpleConnection *gc, gpointer null)
{
  g_idle_add (cb_auto_join_chats, gc);

  return TRUE;
}


static gint
cb_chatty_blist_sort_contacts (GtkListBoxRow *row1,
                               GtkListBoxRow *row2,
                               gpointer       user_data)
{
  char *name1, *name2;
  int   result;

  g_object_get (row1, "name", &name1, NULL);
  g_object_get (row2, "name", &name2, NULL);

  result = g_strcmp0 (name1, name2);

  g_free (name1);
  g_free (name2);

  return result;
}


static void
cb_num_rows_changed (GtkContainer *container,
                     GtkWidget    *widget,
                     gpointer      user_data)
{
  ChattyWindow *window;

  window = chatty_utils_get_window ();

  if (chatty_blist_list_has_children (CHATTY_LIST_CHATS)) {
    chatty_window_set_overlay_visible (window, FALSE);
  } else {
    chatty_window_set_overlay_visible (window, TRUE);
  }
}


// *** end callbacks


static PurpleBlistNode *
chatty_get_selected_node (void) {
  ChattyContactRow *row;
  PurpleBlistNode  *node;

  row = CHATTY_CONTACT_ROW (gtk_list_box_get_selected_row (chatty_get_chats_list()));

  if (row != NULL) {
    g_object_get (row, "data", &node, NULL);
    return node;
  } else {
    return NULL;
  }
}


gboolean
chatty_blist_protocol_is_sms (PurpleAccount *account) 
{
  const gchar *protocol_id;

  g_return_val_if_fail (account != NULL, FALSE);

  protocol_id = purple_account_get_protocol_id (account);

  if (g_strcmp0 (protocol_id, "prpl-mm-sms") == 0) {
    return TRUE; 
  } else {
    return FALSE;
  }
}


void
chatty_blist_refresh (void)
{
  PurpleBlistNode *node;
  PurpleBuddyList *list;

  list = purple_get_blist ();
  node = list->root;

  while (node)
  {
    if (PURPLE_BLIST_NODE_IS_BUDDY (node) || PURPLE_BLIST_NODE_IS_CHAT (node)) {
      chatty_blist_update (list, node);
    }

    node = purple_blist_node_next (node, FALSE);
  }
}


/**
 * chatty_blist_list_has_children:
 *
 * Returns 0 if chats list is empty
 *
 */
gboolean
chatty_blist_list_has_children (int list_type)
{
  gboolean    result;

  if (list_type == CHATTY_LIST_CHATS) {
    result = gtk_list_box_get_row_at_index(chatty_get_chats_list (), 0) != NULL;
  } else if (list_type == CHATTY_LIST_CONTACTS) {
    result = gtk_list_box_get_row_at_index(chatty_get_contacts_list (), 0) != NULL;
  }

  return result;
}


/**
 * chatty_blist_chat_list_select_first:
 *
 * Activate the first entry in the chats list
 *
 */
void
chatty_blist_chat_list_select_first (void)
{
  ChattyWindow  *window;
  GtkListBoxRow *row;

  window = chatty_utils_get_window ();

  row = gtk_list_box_get_row_at_index (chatty_get_chats_list (), 0);

  if (row != NULL) {
    row_selected_cb (chatty_get_chats_list (), row, NULL);
    gtk_list_box_select_row (chatty_get_chats_list (), row);
  } else {
    // The chats list is empty, go back to initial view
    chatty_window_update_sub_header_titlebar (window, NULL, NULL);
    chatty_window_change_view (window, CHATTY_VIEW_CHAT_LIST);
  }
}


/**
 * chatty_blist_add_buddy_from_uri:
 *
 * @uri: a const char
 *
 * called from chatty_application_open()
 * in chatty-application.c
 *
 */
void
chatty_blist_add_buddy_from_uri (const char *uri)
{
  ChattyWindow  *window;
  PurpleAccount *account;
  PurpleBuddy   *buddy;
  char          *who = NULL;
  const char    *folks_id;
  const char    *alias;

  account = purple_accounts_find ("SMS", "prpl-mm-sms");

  if (!purple_account_is_connected (account)) {
    return;
  }

  window = chatty_utils_get_window ();

  who = chatty_utils_check_phonenumber (uri);

  folks_id = chatty_folks_has_individual_with_phonenumber (uri);
  alias = chatty_folks_get_individual_name_by_id (folks_id);

  g_return_if_fail (who != NULL);

  buddy = purple_find_buddy (account, who);

  if (!buddy) {
    buddy = purple_buddy_new (account, who, alias);

    purple_blist_add_buddy (buddy, NULL, NULL, NULL);
  }

  if (!purple_buddy_icons_node_has_custom_icon (PURPLE_BLIST_NODE(buddy))) {
    chatty_folks_set_purple_buddy_data (folks_id, account, g_strdup (who));
  }

  purple_blist_node_set_bool (PURPLE_BLIST_NODE(buddy), "chatty-autojoin", TRUE);

  chatty_conv_im_with_buddy (account, g_strdup (who));

  chatty_window_set_new_chat_dialog_visible (window, FALSE);

  chatty_window_change_view (window, CHATTY_VIEW_MESSAGE_LIST);

  g_free (who);
}


/**
 * chatty_blist_contact_list_add_buddy:
 *
 * Add active chat buddy to contacts-list
 *
 * called from view_msg_list_cmd_add_contact
 * in chatty-popover-actions.c
 *
 */
void
chatty_blist_contact_list_add_buddy (void)
{
  ChattyWindow       *window;
  PurpleAccount      *account;
  PurpleConversation *conv;
  PurpleBuddy        *buddy;
  GtkWidget          *convs_notebook;
  const char         *who;
  const char         *folks_id;             
  g_autofree gchar   *number;
  
  buddy = PURPLE_BUDDY (chatty_get_selected_node ());
  g_return_if_fail (buddy != NULL);

  window = chatty_utils_get_window ();

  convs_notebook = chatty_window_get_convs_notebook (window);

  conv = chatty_conv_container_get_active_purple_conv (GTK_NOTEBOOK(convs_notebook));

  account = purple_conversation_get_account (conv);
  purple_account_add_buddy (account, buddy);
  purple_blist_node_remove_setting (PURPLE_BLIST_NODE(buddy), "chatty-unknown-contact");
  purple_blist_node_set_bool (PURPLE_BLIST_NODE (buddy), "chatty-notifications", TRUE);

  if (chatty_blist_protocol_is_sms (account)) {
    who = purple_buddy_get_name (buddy);

    number = chatty_utils_check_phonenumber (who);

    folks_id = chatty_folks_has_individual_with_phonenumber (number);

    if (folks_id) {
      chatty_dbus_gc_write_contact (who, number);
    }
  }
}


/**
 * chatty_blist_gnome_contacts_add_buddy:
 *
 * Add active chat buddy to GNOME contacts
 *
 * called from view_msg_list_cmd_add_gnome_contact
 * in chatty-popover-actions.c
 *
 */
void
chatty_blist_gnome_contacts_add_buddy (void)
{
  PurpleBuddy        *buddy;
  PurpleContact      *contact;
  const char         *who;
  const char         *alias;
  g_autofree gchar   *number;
  
  buddy = PURPLE_BUDDY (chatty_get_selected_node ());
  g_return_if_fail (buddy != NULL);

  who = purple_buddy_get_name (buddy);
  contact = purple_buddy_get_contact (buddy);
  alias = purple_contact_get_alias (contact);

  number = chatty_utils_check_phonenumber (who);

  chatty_dbus_gc_write_contact (alias, number);
}


/**
 * chatty_blist_chat_list_leave_chat:
 *
 * Remove active chat buddy from chats-list
 *
 * called from view_msg_list_cmd_leave in
 * chatty-popover-actions.c
 *
 */
void
chatty_blist_chat_list_leave_chat (void)
{
  ChattyWindow    *window;
  PurpleBlistNode *node;
  ChattyBlistNode *ui;

  node = chatty_get_selected_node ();

  window = chatty_utils_get_window ();

  if (node == NULL) {
    chatty_window_change_view (window, CHATTY_VIEW_CHAT_LIST);

    return;
  }

  if (node) {
    ui = node->ui_data;

    purple_blist_node_set_bool (node, "chatty-autojoin", FALSE);
    purple_conversation_destroy (ui->conv.conv);
  }

  if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
    chatty_blist_chats_remove_node (node);
  }

  chatty_blist_chat_list_select_first ();
}


/**
 * chatty_blist_chat_list_remove_buddy:
 *
 * Remove active chat buddy from chats-list
 *
 * called from view_msg_list_cmd_delete in
 * chatty-popover-actions.c
 *
 */
void
chatty_blist_chat_list_remove_buddy (void)
{
  ChattyWindow    *window;
  PurpleBlistNode *node;
  PurpleBuddy     *buddy;
  ChattyBlistNode *ui;
  PurpleChat      *chat;
  GtkWidget       *dialog;
  GHashTable      *components;
  const char      *name;
  const char      *text;
  const char      *sub_text;
  int              response;
  const char      *conv_name;

  node = chatty_get_selected_node ();

  window = chatty_utils_get_window ();

  if (node == NULL) {
    chatty_window_change_view (window, CHATTY_VIEW_CHAT_LIST);
    
    return;
  }

  ui = node->ui_data;

  if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
    chat = (PurpleChat*)node;
    name = purple_chat_get_name (chat);
    text = _("Disconnect group chat");
    sub_text = _("This removes chat from chats list");
  } else {
    buddy = (PurpleBuddy*)node;
    name = purple_buddy_get_alias (buddy);
    text = _("Delete chat with");
    sub_text = _("This deletes the conversation history");
  }

  dialog = gtk_message_dialog_new (GTK_WINDOW(window),
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   "%s %s",
                                   text, name);

  gtk_dialog_add_buttons (GTK_DIALOG(dialog),
                          _("Cancel"),
                          GTK_RESPONSE_CANCEL,
                          _("Delete"),
                          GTK_RESPONSE_OK,
                          NULL);

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG(dialog),
                                            "%s",
                                            sub_text);

  gtk_dialog_set_default_response (GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
  gtk_window_set_position (GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

  response = gtk_dialog_run (GTK_DIALOG(dialog));

  if (response == GTK_RESPONSE_OK) {
    if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
      chatty_history_delete_im(buddy->account->username, buddy->name);

      purple_account_remove_buddy (buddy->account, buddy, NULL);
      purple_blist_remove_buddy (buddy);
      purple_conversation_destroy (ui->conv.conv);

      chatty_window_update_sub_header_titlebar (window, NULL, "");
    } else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
      conv_name = purple_conversation_get_name(ui->conv.conv);
      chatty_history_delete_chat(ui->conv.conv->account->username, conv_name);
      // TODO: LELAND: Is this the right place? After recreating a recently
      // deleted chat (same session), the conversation is still in memory
      // somewhere and when re-joining the same chat, the db is not re-populated
      // (until next app session) since there is no server call. Ask @Andrea

      components = purple_chat_get_components (chat);
      g_hash_table_steal (components, "history_since");
      purple_blist_remove_chat (chat);
    }

    chatty_blist_chat_list_select_first ();

    chatty_window_change_view (window, CHATTY_VIEW_CHAT_LIST);
  }

  gtk_widget_destroy (dialog);
}


/**
 * chatty_blist_returned_from_chat:
 *
 * Clears 'selected_node' which is evaluated to
 * block the counting of pending messages
 * while chatting with this node
 *
 * Called from chatty_back_action in
 * chatty-window.c
 */
void
chatty_blist_returned_from_chat (void)
{
  gtk_list_box_unselect_all (chatty_get_chats_list ());
}


/**
 * chatty_blist_set_chat_options:
 *
 * Disable chat options if blist is empty
 *
 */
static void
chatty_blist_set_chat_options (void)
{
  ChattyWindow *window;
  gboolean      sensitive;

  window = chatty_utils_get_window ();

  sensitive = chatty_blist_list_has_children (CHATTY_LIST_CHATS);

  chatty_window_set_header_sub_menu_button_sensitive (window, sensitive);
}


/**
 * chatty_blist_contacts_remove_node:
 * @node: a PurpleBlistNode
 *
 * Removes a node in the contacts list
 *
 */
static void
chatty_blist_contacts_remove_node (PurpleBlistNode *node)
{
  ChattyBlistNode *chatty_node = node->ui_data;

  if (!chatty_node || !chatty_node->row_contact) {
    return;
  }

  gtk_widget_destroy (GTK_WIDGET (chatty_node->row_contact));
  chatty_node->row_contact = NULL;
}


/**
 * chatty_blist_chats_remove_node:
 * @node:   a PurpleBlistNode
 *
 * Removes a node in the chats list
 *
 */
static void
chatty_blist_chats_remove_node (PurpleBlistNode *node)
{
  ChattyBlistNode *chatty_node = node->ui_data;

  if (!chatty_node)
    return;

  g_free(chatty_node->conv.last_message);
  g_free(chatty_node->conv.last_msg_timestamp);
  chatty_node->conv.last_message = NULL;
  chatty_node->conv.last_msg_timestamp = NULL;

  if (!chatty_node->row_chat)
    return;

  gtk_widget_destroy (GTK_WIDGET (chatty_node->row_chat));
  chatty_node->row_chat = NULL;

  chatty_blist_set_chat_options ();
}


static void *
chatty_blist_get_handle (void) {
  static int handle;

  return &handle;
}


/**
 * chatty_blist_join_group_chat:
 * @account:        a PurpleAccount
 * @group_chat_id:  a const char
 * @alias:          a const char
 * @pwd:            a const char
 *
 * Filters the current row according to entry-text
 *
 */
void
chatty_blist_join_group_chat (PurpleAccount *account,
                              const char    *group_chat_id,
                              const char    *room_alias,
                              const char    *user_alias,
                              const char    *pwd)
{
  PurpleChat               *chat;
  PurpleGroup              *group;
  PurpleConnection         *gc;
  PurplePluginProtocolInfo *info;
  GHashTable               *hash = NULL;

  if (!purple_account_is_connected (account) || !group_chat_id) {
    return;
  }

  gc = purple_account_get_connection (account);

  info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl (gc));

  if (info->chat_info_defaults != NULL) {
    hash = info->chat_info_defaults(gc, group_chat_id);
  }

  if (*user_alias != '\0') {
    g_hash_table_replace (hash, "handle", g_strdup (user_alias));
  }

  chat = purple_chat_new (account, group_chat_id, hash);

  if (chat != NULL) {
    if ((group = purple_find_group ("Chats")) == NULL) {
      group = purple_group_new ("Chats");
      purple_blist_add_group (group, NULL);
    }

    purple_blist_add_chat (chat, group, NULL);
    purple_blist_alias_chat (chat, room_alias);
    purple_blist_node_set_bool ((PurpleBlistNode*)chat,
                                "chatty-autojoin",
                                TRUE);

    chatty_conv_join_chat (chat);
  }
}


/**
 * chatty_blist_create_chat_list:
 * @list:  a PurpleBuddyList
 *
 * Sets up view with chat list treeview
 * Function is called from chatty_blist_show.
 *
 */
static void
chatty_blist_create_chat_list (void)
{
  ChattyWindow   *window;
  GtkSearchEntry *search_entry;
  GtkWidget      *listbox;

  window = chatty_utils_get_window ();
  search_entry = GTK_SEARCH_ENTRY(chatty_window_get_search_entry (window));
  listbox = chatty_window_get_chats_listbox (window);

  g_signal_connect (search_entry,
                    "search-changed",
                    G_CALLBACK (cb_chats_search_entry_changed),
                    listbox);

  gtk_list_box_set_filter_func (GTK_LIST_BOX(listbox), filter_chat_list_cb, search_entry, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX(listbox), chatty_blist_sort, NULL, NULL);

  g_signal_connect (listbox,
                    "row-activated",
                    G_CALLBACK (row_selected_cb),
                    NULL);

  g_signal_connect (listbox,
                    "add",
                    G_CALLBACK (cb_num_rows_changed),
                    NULL);

  g_signal_connect (listbox,
                    "remove",
                    G_CALLBACK (cb_num_rows_changed),
                    NULL);
}


/**
 * chatty_blist_create_chat_list:
 * @list:  a PurpleBuddyList
 *
 * Sets up view with contact list treeview
 * Function is called from chatty_blist_show.
 *
 */
static void
chatty_blist_create_contact_list (void)
{
  ChattyWindow *window;
  GtkWidget    *dialog;
  GtkListBox   *list;
  GtkWidget    *search_entry_contacts;

  window = chatty_utils_get_window ();

  dialog = chatty_window_get_new_chat_dialog (window);

  list = GTK_LIST_BOX(chatty_new_chat_get_list_contacts ((ChattyNewChatDialog*)dialog));

  search_entry_contacts = chatty_new_chat_get_search_entry (CHATTY_NEW_CHAT_DIALOG(dialog));

  g_signal_connect (search_entry_contacts,
                    "search-changed",
                    G_CALLBACK (cb_search_entry_contacts_changed),
                    list);

  gtk_list_box_set_filter_func (GTK_LIST_BOX(list), filter_contacts_list_cb, search_entry_contacts, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX(list), cb_chatty_blist_sort_contacts, NULL, NULL);

  g_signal_connect (list,
                    "row-activated",
                    G_CALLBACK (row_selected_cb),
                    NULL);
}


void
chatty_blist_enable_folks_contacts (void) 
{
  GtkListBox *listbox;

  listbox = chatty_get_contacts_list ();

  if (listbox != NULL)
    chatty_folks_init (listbox);
}


/**
 * chatty_blist_show:
 * @list:  a PurpleBuddyList
 *
 * Create chat and contact lists and
 * setup signal handlers for conversation
 * and buddy status.
 *
 * Function is called via PurpleBlistUiOps.
 *
 */
static void
chatty_blist_show (PurpleBuddyList *list)
{
  void  *handle;

  list_refresh_timer = purple_timeout_add_seconds (30,
                                                   (GSourceFunc)cb_chatty_blist_refresh_timer,
                                                   list);

  purple_blist_set_visible (TRUE);

  handle = chatty_blist_get_handle();
  purple_signal_emit (handle, "chatty-blist-created", list);
}


/**
 * chatty_blist_remove:
 * @list:  a PurpleBuddyList
 * @node:  a PurpleBlistNode
 *
 * Removes a blist node from the list.
 * Function is called via PurpleBlistUiOps.
 *
 */
static void
chatty_blist_remove (PurpleBuddyList *list,
                     PurpleBlistNode *node)
{
  ChattyBlistNode *chatty_node = node->ui_data;

  purple_request_close_with_handle (node);

  chatty_blist_chats_remove_node (node);
  chatty_blist_contacts_remove_node (node);

  if (chatty_node) {
    if (chatty_node->recent_signonoff_timer > 0) {
      purple_timeout_remove(chatty_node->recent_signonoff_timer);
    }

    purple_signals_disconnect_by_handle (node->ui_data);

    g_free (node->ui_data);
    node->ui_data = NULL;
  }
}


static gint
chatty_blist_sort (GtkListBoxRow *row1,
                   GtkListBoxRow *row2,
                   gpointer user_data)
{
  PurpleBlistNode *node1;
  ChattyBlistNode *chatty_node1;
  PurpleBlistNode *node2;
  ChattyBlistNode *chatty_node2;

  g_object_get (row1, "data", &node1, NULL);
  chatty_node1 = node1->ui_data;

  g_object_get (row2, "data", &node2, NULL);
  chatty_node2 = node2->ui_data;

  if (chatty_node1 != NULL && chatty_node2 != NULL) {
    return difftime (chatty_node2->conv.last_msg_ts_raw, chatty_node1->conv.last_msg_ts_raw);
  }
  return 0;
}


/**
 * chatty_blist_contacts_add_contact:
 * @buddy: a PurpleBuddy
 * @node:  a PurpleBlistNode
 *
 * Inserts a contact in the contacts list
 *
 */
static void
chatty_blist_contacts_update_node (PurpleBuddy     *buddy,
                                   PurpleBlistNode *node)
{
  GtkListBox    *listbox;
  GdkPixbuf     *avatar;
  gchar         *name = NULL;
  g_autofree char *alias = NULL;
  const gchar   *account_name;
  PurpleAccount *account;
  gboolean       blur;

  PurplePresence *presence = purple_buddy_get_presence (buddy);

  ChattyBlistNode *chatty_node = node->ui_data;

  account = purple_buddy_get_account (buddy);
  account_name = purple_account_get_username (account);

  if (!PURPLE_BLIST_NODE_IS_BUDDY (node) || purple_account_is_disconnected (account)) {
    return;
  }

  // Do not add unknown contacts to the list
  if (purple_blist_node_get_bool (PURPLE_BLIST_NODE(buddy),
                                  "chatty-unknown-contact")) {
    return;
  }

  alias = chatty_utils_jabber_id_strip (purple_buddy_get_alias (buddy));

  if (!PURPLE_BUDDY_IS_ONLINE(buddy)) {
    blur = TRUE;
  } else {
    blur = FALSE;
  }

  avatar = chatty_icon_get_buddy_icon ((PurpleBlistNode *)buddy,
                                       alias,
                                       CHATTY_ICON_SIZE_MEDIUM,
                                       chatty_blist_protocol_is_sms (account) ?
                                       CHATTY_COLOR_GREEN : CHATTY_COLOR_BLUE,
                                       blur);

  if (chatty_settings_get_blur_idle_buddies (chatty_settings_get_default ()) &&
      purple_presence_is_idle (presence)) {

    chatty_icon_do_alphashift (avatar, 77);
  }

  listbox = chatty_get_contacts_list ();

  /* Create a new row or update the row if it already exists */
  if (chatty_node->row_contact == NULL) {
    chatty_node->row_contact = CHATTY_CONTACT_ROW (chatty_contact_row_new ((gpointer) node,
                                                    avatar,
                                                    alias,
                                                    account_name,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    FALSE));

    gtk_list_box_row_set_selectable (GTK_LIST_BOX_ROW (chatty_node->row_contact), FALSE);
    gtk_widget_show (GTK_WIDGET (chatty_node->row_contact));
    gtk_container_add (GTK_CONTAINER (listbox), GTK_WIDGET (chatty_node->row_contact));
  } else {
    g_object_set (chatty_node->row_contact,
                  "avatar", avatar,
                  "name", alias,
                  "description", account_name,
                  NULL);
  }

  gtk_list_box_invalidate_sort (listbox);

  if (avatar) {
    g_object_unref (avatar);
  }

  g_free (name);
}


/**
 * chatty_blist_contacts_update_group_chat:
 * @buddy: a PurpleBuddy
 * @node:  a PurpleBlistNode
 *
 * Inserts a contact in the contacts list
 *
 */
static void
chatty_blist_contacts_update_group_chat (PurpleBlistNode *node)
{
  GtkListBox    *listbox;
  GdkPixbuf     *avatar;
  PurpleChat    *chat;
  gchar         *name = NULL;
  const gchar   *chat_name;
  const gchar   *account_name;

  ChattyBlistNode *chatty_node = node->ui_data;

  if (!PURPLE_BLIST_NODE_IS_CHAT (node)) {
    return;
  }

  chat = (PurpleChat*)node;

  if(!purple_account_is_connected (chat->account)) {
    return;
  }

  avatar = chatty_icon_get_buddy_icon (node,
                                       NULL,
                                       CHATTY_ICON_SIZE_MEDIUM,
                                       CHATTY_COLOR_BLUE,
                                       FALSE);

  account_name = purple_account_get_username (chat->account);
  chat_name = purple_chat_get_name (chat);

  listbox = chatty_get_contacts_list ();

  /* Create a new row or update the row if it already exists */
  if (chatty_node->row_contact == NULL) {
    chatty_node->row_contact = CHATTY_CONTACT_ROW (chatty_contact_row_new ((gpointer) node,
                                                    avatar,
                                                    chat_name,
                                                    account_name,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    FALSE));
    gtk_widget_show (GTK_WIDGET (chatty_node->row_contact));
    gtk_container_add (GTK_CONTAINER (listbox), GTK_WIDGET (chatty_node->row_contact));
  } else {
    g_object_set (chatty_node->row_contact,
                  "avatar", avatar,
                  "name", chat_name,
                  "description", account_name,
                  NULL);
  }

  gtk_list_box_invalidate_sort (listbox);

  if (avatar) {
    g_object_unref (avatar);
  }

  g_free (name);
}


/**
 * chatty_blist_chats_update_node:
 * @buddy: a PurpleBuddy
 * @node:  a PurpleBlistNode
 *
 * Inserts or updates a buddy node in the chat list
 *
 */
static void
chatty_blist_chats_update_node (PurpleBuddy     *buddy,
                                PurpleBlistNode *node)
{
  PurpleAccount    *account;
  PurpleContact    *contact;
  GtkListBox       *listbox;
  GdkPixbuf        *avatar;
  g_autofree gchar *name = NULL;
  g_autofree gchar *last_msg = NULL;
  g_autofree gchar *last_msg_text = NULL;
  g_autofree gchar *last_msg_ts = NULL;
  g_autofree gchar *unread_messages = NULL;
  g_autofree gchar *last_message_striped = NULL;
  g_autofree gchar *alias = NULL;
  const gchar      *tag;
  gboolean          blur;
  gboolean          muted;

  PurplePresence *presence = purple_buddy_get_presence (buddy);

  ChattyBlistNode *chatty_node = node->ui_data;

  account = purple_buddy_get_account (buddy);

  if (!PURPLE_BLIST_NODE_IS_BUDDY (node)) {
    return;
  }

  contact = purple_buddy_get_contact (buddy);
  alias = chatty_utils_jabber_id_strip (purple_contact_get_alias (contact));

  if (chatty_settings_get_greyout_offline_buddies (chatty_settings_get_default ()) &&
      !PURPLE_BUDDY_IS_ONLINE(buddy)) {

    blur = TRUE;
  } else {
    blur = FALSE;
  }

  avatar = chatty_icon_get_buddy_icon (node,
                                       alias,
                                       CHATTY_ICON_SIZE_MEDIUM,
                                       chatty_blist_protocol_is_sms (account) ?
                                       CHATTY_COLOR_GREEN : CHATTY_COLOR_BLUE,
                                       blur);

  if (chatty_settings_get_blur_idle_buddies (chatty_settings_get_default ()) &&
      purple_presence_is_idle (presence)) {

    chatty_icon_do_alphashift (avatar, 77);
  }

  if (chatty_node->conv.last_message_dir == MSG_IS_INCOMING) {
    tag = "";
  } else {
    tag = _("Me: ");
  }

  if (chatty_node->conv.last_message == NULL) {
    chatty_node->conv.last_message = g_strdup("");
  }

  // FIXME: Don't hard code the color it should read it from the theme
  last_message_striped = purple_markup_strip_html (chatty_node->conv.last_message);
  last_msg_text = g_markup_printf_escaped ("<span color='#3584e4'>%s</span><span alpha='55%%'>%s</span>",
                                           tag,
                                           last_message_striped);

  last_msg = chatty_utils_strip_cr_lf (last_msg_text);

  last_msg_ts = chatty_utils_time_ago_in_words (chatty_node->conv.last_msg_ts_raw,
                                                CHATTY_UTILS_TIME_AGO_SHOW_DATE);

  if (purple_blist_node_get_bool (PURPLE_BLIST_NODE(buddy), "chatty-unknown-contact") &&
      chatty_settings_get_indicate_unkown_contacts (chatty_settings_get_default ())) {
    name = g_markup_printf_escaped ("<span color='#FF3333'>%s</span>", alias);
  } else {
    name = g_markup_printf_escaped ("%s", alias);
  }

  muted = !purple_blist_node_get_bool (PURPLE_BLIST_NODE(buddy), "chatty-notifications");

  if (chatty_node->conv.pending_messages) {
    unread_messages = g_strdup_printf ("%d", chatty_node->conv.pending_messages);
  }

  listbox = chatty_get_chats_list ();

  /* Create a new row or update the row if it already exists */
  if (chatty_node->row_chat == NULL) {
    chatty_node->row_chat = CHATTY_CONTACT_ROW (chatty_contact_row_new ((gpointer) node,
                                                    avatar,
                                                    name,
                                                    last_msg,
                                                    last_msg_ts,
                                                    unread_messages,
                                                    NULL,
                                                    NULL,
                                                    muted));
                                                    
    gtk_widget_show (GTK_WIDGET (chatty_node->row_chat));
    gtk_container_add (GTK_CONTAINER (listbox), GTK_WIDGET (chatty_node->row_chat));
    chatty_blist_set_chat_options ();
  } else {
    g_object_set (chatty_node->row_chat,
                  "avatar", avatar,
                  "name", name,
                  "description", last_msg,
                  "timestamp", last_msg_ts,
                  "message_count", unread_messages,
                  NULL);
  }

  gtk_list_box_invalidate_sort (listbox);

  if (avatar) {
    g_object_unref (avatar);
  }
}


/**
 * chatty_blist_chats_update_group_chat:
 * @buddy: a PurpleBuddy
 * @node:  a PurpleBlistNode
 *
 * Inserts or updates a group chat node in the chat list
 *
 */
static void
chatty_blist_chats_update_group_chat (PurpleBlistNode *node)
{
  GtkListBox       *listbox;
  PurpleChat       *chat;
  GdkPixbuf        *avatar = NULL;
  g_autofree gchar *name = NULL;
  g_autofree gchar *last_msg_text = NULL;
  g_autofree gchar *last_msg_ts = NULL;
  g_autofree gchar *unread_messages = NULL;
  g_autofree gchar *last_message_striped = NULL;
  const gchar      *chat_name;
  gboolean          muted;

  ChattyBlistNode *chatty_node = node->ui_data;

  if (!PURPLE_BLIST_NODE_IS_CHAT (node)) {
    return;
  }

  chat = (PurpleChat*)node;

  if(!purple_account_is_connected (chat->account)) {
    return;
  }

  avatar = chatty_icon_get_buddy_icon (node,
                                       NULL,
                                       CHATTY_ICON_SIZE_MEDIUM,
                                       CHATTY_COLOR_BLUE,
                                       FALSE);

  chat_name = purple_chat_get_name (chat);

  if (chatty_node->conv.last_message == NULL) {
    chatty_node->conv.last_message = g_strdup("");
  }

  last_message_striped = purple_markup_strip_html (chatty_node->conv.last_message);
  // FIXME: Don't hard code the color it should read it from the theme
  last_msg_text = g_markup_printf_escaped ("<span color='#3584e4'>Group Chat: </span>%s",
                                           last_message_striped);

  muted = !purple_blist_node_get_bool (node, "chatty-notifications");

  g_strdup (chatty_node->conv.last_msg_timestamp);

  if (chatty_node->conv.pending_messages) {
    unread_messages = g_strdup_printf ("%d", chatty_node->conv.pending_messages);
  }

  listbox = chatty_get_chats_list ();

  /* Create a new row or update the row if it already exists */
  if (chatty_node->row_chat == NULL) {
    chatty_node->row_chat = CHATTY_CONTACT_ROW (chatty_contact_row_new ((gpointer) node,
                                                    avatar,
                                                    chat_name,
                                                    last_msg_text,
                                                    last_msg_ts,
                                                    unread_messages,
                                                    NULL,
                                                    NULL,
                                                    muted));

    gtk_widget_show (GTK_WIDGET (chatty_node->row_chat));
    gtk_container_add (GTK_CONTAINER (listbox), GTK_WIDGET (chatty_node->row_chat));
    chatty_blist_set_chat_options ();
  } else {
    g_object_set (chatty_node->row_chat,
                  "avatar", avatar,
                  "name", chat_name,
                  "description", last_msg_text,
                  "timestamp", last_msg_ts,
                  "message_count", unread_messages,
                  NULL);
  }

  gtk_list_box_invalidate_sort (listbox);

  if (avatar) {
    g_object_unref (avatar);
  }
}


/**
 * chatty_blist_update_buddy:
 * @blist: a PurpleBuddyList
 * @node:  a PurpleBlistNode
 *
 * Updates buddy nodes.
 * Function is called from #chatty_blist_update.
 *
 */
static void
chatty_blist_update_buddy (PurpleBuddyList *list,
                           PurpleBlistNode *node)
{
  PurpleBuddy             *buddy;
  g_autofree ChattyLog    *log_data = NULL;
  ChattyBlistNode         *ui;
  PurpleAccount           *account;
  const char              *username;
  g_autofree char         *who;
  gchar                   *iso_timestamp;
  struct tm               *timeinfo;
  char                     message_exists;

  g_return_if_fail (PURPLE_BLIST_NODE_IS_BUDDY(node));

  buddy = (PurpleBuddy*)node;

  account = purple_buddy_get_account (buddy);

  username = purple_account_get_username (account);
  who = chatty_utils_jabber_id_strip (purple_buddy_get_name (buddy));
  log_data = g_new0(ChattyLog, 1);

  message_exists = chatty_history_get_im_last_message (username, who, log_data);

  if (purple_blist_node_get_bool (node, "chatty-autojoin") &&
      purple_account_is_connected (buddy->account) &&
      message_exists) {

    timeinfo = localtime (&log_data->epoch);
    iso_timestamp = g_malloc0(MAX_GMT_ISO_SIZE * sizeof(char));
    g_return_if_fail (strftime (iso_timestamp,
                                MAX_GMT_ISO_SIZE * sizeof(char),
                                "%I:%M %p",
                                timeinfo));

    ui = node->ui_data;
    g_free(ui->conv.last_message);
    g_free(ui->conv.last_msg_timestamp);
    ui->conv.last_message = log_data->msg;
    ui->conv.last_message_dir = log_data->dir;
    ui->conv.last_msg_ts_raw = log_data->epoch;
    ui->conv.last_msg_timestamp = iso_timestamp;

    chatty_blist_chats_update_node (buddy, node);
  } else {
    chatty_blist_chats_remove_node (node);
  }

  if (chatty_blist_protocol_is_sms (account)) {
    if (chatty_folks_has_individual_with_phonenumber (who)) {
      chatty_blist_contacts_remove_node (node);
    } else {
      chatty_blist_contacts_update_node (buddy, node);
    }
  } else {
    chatty_blist_contacts_update_node (buddy, node);
  }
}


/**
 * chatty_blist_update:
 * @blist: a PurpleBuddyList
 * @node:  a PurpleBlistNode
 *
 * Initiates blist update.
 * Calls #chatty_blist_update_buddy()
 *
 */
static void
chatty_blist_update (PurpleBuddyList *list,
                     PurpleBlistNode *node)
{
  if (!node) {
    return;
  }

  if (node->ui_data == NULL) {
    chatty_blist_new_node (node);
  }

  switch (node->type) {
    case PURPLE_BLIST_BUDDY_NODE:
      chatty_blist_update_buddy (list, node);
      break;
    case PURPLE_BLIST_CHAT_NODE:
      chatty_blist_contacts_update_group_chat (node);

      if (purple_blist_node_get_bool(node, "chatty-autojoin")) {
        chatty_blist_chats_update_group_chat (node);
      }
      break;
    case PURPLE_BLIST_CONTACT_NODE:
    case PURPLE_BLIST_GROUP_NODE:
    case PURPLE_BLIST_OTHER_NODE:
    default:
      return;
  }
}


/**
 * chatty_blist_destroy:
 * @blist: a PurpleBuddyList
 *
 * Called before a blist is freed.
 * Function is called via #PurpleBlistUiOps.
 *
 */
static void
chatty_blist_destroy (PurpleBuddyList *list)
{
  PurpleBlistNode *node;

  list = purple_get_blist ();
  node = list->root;

  if (list_refresh_timer) {
    purple_timeout_remove (list_refresh_timer);

    list_refresh_timer = 0;
  }

  while (node)
  {
    chatty_blist_contacts_remove_node (node);
    chatty_blist_chats_remove_node (node);
    g_free (node->ui_data);
    node = purple_blist_node_next (node, FALSE);
  }
}


/**
 * chatty_blist_request_add_buddy:
 * @account:  a PurpleAccount
 * @username: a const char
 * @group:    a const char
 * @alias:    a const char
 *
 * Invokes the dialog for adding a buddy to the blist.
 * Function is called via #PurpleBlistUiOps.
 *
 */
static void
chatty_blist_request_add_buddy (PurpleAccount *account,
                                const char    *username,
                                const char    *group,
                                const char    *alias)
{
  PurpleBuddy     *buddy;
  const char      *account_name;

  buddy = purple_find_buddy (account, username);

  if (buddy == NULL) {
    buddy = purple_buddy_new (account, username, alias);

    purple_blist_add_buddy (buddy, NULL, NULL, NULL);
    purple_blist_node_set_bool (PURPLE_BLIST_NODE(buddy), "chatty-notifications", TRUE);
  }

  purple_account_add_buddy (account, buddy);

  account_name = purple_account_get_username (account);

  g_debug ("chatty_blist_request_add_buddy: %s  %s  %s",
           account_name, username, alias);
}


/**
 * chatty_blist_new_node:
 * @node: a PurpleBlistNode
 *
 * Creates a new chatty_blist_node.
 * Function is called via PurpleBlistUiOps.
 *
 */
static void
chatty_blist_new_node (PurpleBlistNode *node)
{
  node->ui_data = g_new0 (struct _chatty_blist_node, 1);
}


/**
 * PurpleBlistUiOps:
 *
 * The interface struct for libpurple blist events.
 * Callbackhandler for the UI are assigned here.
 *
 */
static PurpleBlistUiOps blist_ui_ops =
{
  NULL,
  chatty_blist_new_node,
  chatty_blist_show,
  chatty_blist_update,
  chatty_blist_remove,
  chatty_blist_destroy,
  NULL,
  chatty_blist_request_add_buddy,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

PurpleBlistUiOps *
chatty_blist_get_ui_ops (void)
{
  return &blist_ui_ops;
}


/**
 * chatty_buddy_list_init:
 *
 * Sets purple blist preferenz values and
 * defines libpurple signal callbacks
 *
 */
void chatty_blist_init (void)
{
  static int handle;
  void *conv_handle;
  ChattySettings *settings;
  void *chatty_blist_handle = chatty_blist_get_handle();

  chatty_blist_create_chat_list ();
  chatty_blist_create_contact_list ();

  settings = chatty_settings_get_default ();
  g_object_connect (settings,
                    "signal::notify::indicate-unkown-contacts", G_CALLBACK (cb_chatty_prefs_changed), NULL,
                    "signal::notify::blur-idle-buddies", G_CALLBACK (cb_chatty_prefs_changed), NULL,
                    "signal::notify::greyout-offline-buddies", G_CALLBACK (cb_chatty_prefs_changed), NULL,
                    NULL);

  purple_signal_register (chatty_blist_handle,
                          "chatty-blist-created",
                          purple_marshal_VOID__POINTER,
                          NULL,
                          1,
                          purple_value_new (PURPLE_TYPE_SUBTYPE,
                                            PURPLE_SUBTYPE_BLIST));

  purple_signal_connect_priority (purple_connections_get_handle(),
                                  "autojoin",
                                   &handle,
                                  PURPLE_CALLBACK(cb_do_autojoin),
                                  NULL,
                                  PURPLE_SIGNAL_PRIORITY_HIGHEST);

  purple_signal_connect (purple_blist_get_handle (),
                         "buddy-signed-on",
                         &handle,
                         PURPLE_CALLBACK (cb_buddy_signed_on_off),
                         NULL);

  purple_signal_connect (purple_blist_get_handle (),
                         "buddy-signed-off",
                         &handle,
                         PURPLE_CALLBACK (cb_buddy_signed_on_off),
                         NULL);

  purple_signal_connect (purple_blist_get_handle (),
                         "buddy-status-changed",
                         &handle,
                         PURPLE_CALLBACK (cb_buddy_away),
                         NULL);

  purple_signal_connect (purple_blist_get_handle (),
                         "buddy-idle-changed",
                         &handle,
                         PURPLE_CALLBACK (cb_buddy_idle),
                         NULL);

  purple_signal_connect (purple_blist_get_handle (),
                         "buddy-privacy-changed",
                         &handle,
                         PURPLE_CALLBACK (cb_chatty_blist_update_privacy),
                         NULL);


  conv_handle = purple_connections_get_handle ();

  purple_signal_connect (conv_handle, "signed-on", &handle,
                        PURPLE_CALLBACK(cb_sign_on_off), NULL);
  purple_signal_connect (conv_handle, "signed-off", &handle,
                        PURPLE_CALLBACK(cb_sign_on_off), NULL);

  conv_handle = purple_conversations_get_handle();

  purple_signal_connect (conv_handle, "conversation-updated", &handle,
                         PURPLE_CALLBACK(cb_conversation_updated),
                         NULL);
  purple_signal_connect (conv_handle, "deleting-conversation", &handle,
                         PURPLE_CALLBACK(cb_conversation_deleting),
                         NULL);
  purple_signal_connect (conv_handle, "conversation-created", &handle,
                         PURPLE_CALLBACK(cb_conversation_created),
                         NULL);
  purple_signal_connect (conv_handle,
                         "chat-joined",
                         &handle,
                         PURPLE_CALLBACK(cb_chat_joined),
                         NULL);
}


void 
chatty_blist_disconnect_listbox (GtkWidget *list)
{
  // TODO: chatlist related signals and callback-handlers 
  // need to be moved to chatty-window
  g_signal_handlers_disconnect_by_func (G_OBJECT(list), 
                                        cb_num_rows_changed, 
                                        NULL);
}


void
chatty_blist_uninit (void) {
  purple_signals_unregister_by_instance (chatty_blist_get_handle());
  purple_signals_disconnect_by_handle (chatty_blist_get_handle());
}
