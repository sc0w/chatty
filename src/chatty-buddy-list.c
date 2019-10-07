/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-buddy-list"

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
#define HANDY_USE_UNSTABLE_API
#include <handy.h>
#include <libebook-contacts/libebook-contacts.h>


static void chatty_blist_new_node (PurpleBlistNode *node);

static void chatty_blist_update (PurpleBuddyList *list,
                                 PurpleBlistNode *node);

static void chatty_blist_chats_remove_node (PurpleBlistNode *node);

static void chatty_blist_contacts_remove_node (PurpleBlistNode *node);

static void chatty_blist_update_buddy (PurpleBuddyList *list,
                                       PurpleBlistNode *node);

static gint chatty_blist_sort (GtkListBoxRow *row1, GtkListBoxRow *row2, gpointer user_data);

static GtkListBox *chatty_get_contacts_list (void) {
  return chatty_get_data ()->listbox_contacts;
}

static GtkListBox *chatty_get_chats_list (void) {
  return chatty_get_data ()->listbox_chats;
}

// *** callbacks
static void
row_selected_cb (GtkListBox    *box,
                 GtkListBoxRow *row,
                 gpointer       user_data)
{
  PurpleBlistNode *node;
  PurpleAccount   *account;
  PurpleChat      *chat;
  const char      *chat_name;
  GdkPixbuf       *avatar;
  chatty_data_t   *chatty = chatty_get_data ();

  if (row == NULL)
    return;

  g_object_get (row, "data", &node, NULL);

  if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
    PurpleBuddy *buddy;

    buddy = (PurpleBuddy*)node;
    account = purple_buddy_get_account (buddy);

    gtk_widget_hide (chatty->button_header_chat_info);

    if (purple_blist_node_get_bool (PURPLE_BLIST_NODE(buddy),
                                    "chatty-unknown-contact")) {

      gtk_widget_show (chatty->button_menu_add_contact);
    } else {
      gtk_widget_hide (chatty->button_menu_add_contact);
    }

    purple_blist_node_set_bool (node, "chatty-autojoin", TRUE);

    chatty_conv_im_with_buddy (account,
                               purple_buddy_get_name (buddy));

    gtk_widget_hide (GTK_WIDGET(chatty->dialog_new_chat));

  } else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
    chat = (PurpleChat*)node;
    chat_name = purple_chat_get_name (chat);

    gtk_widget_hide (chatty->button_menu_add_contact);

    chatty_conv_join_chat (chat);

    purple_blist_node_set_bool (node, "chatty-autojoin", TRUE);

    avatar = chatty_icon_get_buddy_icon (node,
                                         NULL,
                                          CHATTY_ICON_SIZE_MEDIUM,
                                          CHATTY_COLOR_GREY,
                                          FALSE);
 
     chatty_window_update_sub_header_titlebar (avatar, chat_name);
     chatty_window_change_view (CHATTY_VIEW_MESSAGE_LIST);
     gtk_widget_hide (GTK_WIDGET(chatty->dialog_new_chat));
 
     g_object_unref (avatar);
   }
}


static void
cb_search_entry_changed (GtkSearchEntry     *entry,
                         GtkListBox *listbox)
{
  gtk_list_box_invalidate_filter (GTK_LIST_BOX (listbox));
}

static gboolean
filter_chat_list_cb (GtkListBoxRow *row, gpointer entry) {
  const gchar *query;
  const gchar *name;

  query = gtk_entry_get_text (GTK_ENTRY (entry));

  g_object_get (row, "name", &name, NULL);

  // TODO: make search case insensitive
  return ((*query == '\0') || (name && strstr (name, query)));
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

  chatty_node->recent_signonoff = FALSE;
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

  chatty_node->recent_signonoff = TRUE;

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
  ui->conv.flags = 0;
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

  if (flag & (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV)) {
    if (!gtk_list_box_row_is_selected (GTK_LIST_BOX_ROW (ui->row_chat))) {
      ui->conv.flags |= CHATTY_BLIST_NODE_HAS_PENDING_MESSAGE;
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

  ui->conv.flags &= ~(CHATTY_BLIST_NODE_HAS_PENDING_MESSAGE |
                      CHATTY_BLIST_CHAT_HAS_PENDING_MESSAGE_WITH_NICK);

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
      ui->conv.flags = 0;

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
    ui->conv.flags = 0;
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
cb_chatty_prefs_change_update_list (const char     *name,
                                    PurplePrefType  type,
                                    gconstpointer   val,
                                    gpointer        data)
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

        prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_find_prpl (purple_account_get_protocol_id (account)));
        components = purple_chat_get_components (chat);
        chatty_conv_add_history_since_component(components, account->username, prpl_info->get_chat_name(components));

        serv_join_chat (purple_account_get_connection (account),
                        purple_chat_get_components (chat));
      }
    }
  }

  return FALSE;
}


static gboolean
cb_do_autojoin (PurpleConnection *gc, gpointer null)
{
  g_idle_add (cb_auto_join_chats, gc);

  return TRUE;
}
// *** end callbacks


static PurpleBlistNode *
chatty_get_selected_node (void) {
  ChattyContactRow *row;
  PurpleBlistNode *node;

  row = CHATTY_CONTACT_ROW (gtk_list_box_get_selected_row (chatty_get_chats_list()));
  g_return_val_if_fail (row != NULL, NULL);

  g_object_get (row, "data", &node, NULL);

  return node;
}


/**
 * chatty_blist_buddy_is_displayable:
 * @buddy:      a PurpleBuddy
 *
 * Determines if a buddy may be displayed
 * in the chat list
 *
 */
static gboolean
chatty_blist_buddy_is_displayable (PurpleBuddy *buddy)
{
  struct _chatty_blist_node *chatty_node;

  if (!buddy) {
    return FALSE;
  }

  chatty_node = ((PurpleBlistNode*)buddy)->ui_data;

  return (purple_account_is_connected (buddy->account) &&
          (purple_presence_is_online (buddy->presence) ||
           (chatty_node && chatty_node->recent_signonoff) ||
           purple_prefs_get_bool (CHATTY_PREFS_ROOT "/blist/show_offline_buddies")));

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
    result = gtk_list_box_get_row_at_index(chatty_get_chats_list (), 1) != NULL;
  } else if (list_type == CHATTY_LIST_CONTACTS) {
    result = gtk_list_box_get_row_at_index(chatty_get_contacts_list (), 1) != NULL;
  }

  return result;
}


/**
 * chatty_blist_chat_list_set_row:
 *
 * Activate the first entry in the chats list
 *
 */
static void
chatty_blist_chat_list_set_row (void)
{
  GtkListBoxRow *row;

  row = gtk_list_box_get_row_at_index(chatty_get_chats_list (), 1);

  if (row != NULL) {
    gtk_list_box_select_row (chatty_get_chats_list (), row);
  } else {
    // The chats list is empty, go back to initial view
    chatty_window_update_sub_header_titlebar (NULL, NULL);
    chatty_window_change_view (CHATTY_VIEW_CHAT_LIST);
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
  PurpleAccount     *account;
  PurpleBuddy       *buddy;
  GdkPixbuf         *avatar;
  EPhoneNumber      *number;
  GtkWindow         *window;
  char              *region;
  char              *who;
  g_autoptr(GError)  err = NULL;

  account = purple_accounts_find ("SMS", "prpl-mm-sms");

  if (!purple_account_is_connected (account)) {
    return;
  }

  region = e_phone_number_get_default_region (NULL);
  number = e_phone_number_from_string (uri, region, &err);

  if (!number || !e_phone_number_is_supported ()) {
    g_warning ("failed to parse %s: %s", uri, err->message);

    return;
  } else {
    who = e_phone_number_to_string (number, E_PHONE_NUMBER_FORMAT_E164);
  }

  g_free (region);
  e_phone_number_free (number);

  buddy = purple_find_buddy (account, who);

  if (!buddy) {
    buddy = purple_buddy_new (account, who, NULL);

    purple_blist_add_buddy (buddy, NULL, NULL, NULL);
  }

  avatar = chatty_icon_get_buddy_icon (PURPLE_BLIST_NODE(buddy),
                                       who,
                                       CHATTY_ICON_SIZE_MEDIUM,
                                       CHATTY_COLOR_GREEN,
                                       FALSE);

  chatty_conv_im_with_buddy (account, g_strdup (who));

  if (avatar) {
    chatty_window_update_sub_header_titlebar (avatar,
                                              who);
  }

  chatty_window_change_view (CHATTY_VIEW_MESSAGE_LIST);

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));

  gtk_window_present (window);

  g_free (who);

  g_object_unref (avatar);
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
  PurpleAccount      *account;
  PurpleConversation *conv;
  PurpleBuddy        *buddy;
  
  chatty_data_t *chatty = chatty_get_data ();

  buddy = PURPLE_BUDDY (chatty_get_selected_node ());
  g_return_if_fail (buddy != NULL);

  conv = chatty_conv_container_get_active_purple_conv (GTK_NOTEBOOK(chatty->pane_view_message_list));

  account = purple_conversation_get_account (conv);
  purple_account_add_buddy (account, buddy);
  purple_blist_node_remove_setting (PURPLE_BLIST_NODE(buddy), "chatty-unknown-contact");
  purple_blist_node_set_bool (PURPLE_BLIST_NODE (buddy), "chatty-notifications", TRUE);
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
  PurpleBlistNode *node;
  ChattyBlistNode *ui;

  node = chatty_get_selected_node ();

  if (node) {
    ui = node->ui_data;

    purple_blist_node_set_bool (node, "chatty-autojoin", FALSE);
    purple_conversation_destroy (ui->conv.conv);
  }

  if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
    chatty_blist_chats_remove_node (node);
  }

  chatty_blist_chat_list_set_row ();
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
  PurpleBlistNode *node;
  PurpleBuddy     *buddy;
  ChattyBlistNode *ui;
  PurpleChat      *chat;
  GtkWidget       *dialog;
  GtkWindow       *window;
  const char      *name;
  const char      *text;
  const char      *sub_text;
  int              response;
  const char      *conv_name;

  node = chatty_get_selected_node ();

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

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
  dialog = gtk_message_dialog_new (window,
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

      chatty_window_update_sub_header_titlebar (NULL, "");
    } else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
      conv_name = purple_conversation_get_name(ui->conv.conv);
      chatty_history_delete_chat(ui->conv.conv->account->username, conv_name);
      // TODO: LELAND: Is this the right place? After recreating a recently
      // deleted chat (same session), the conversation is still in memory
      // somewhere and when re-joining the same chat, the db is not re-populated
      // (until next app session) since there is no server call. Ask @Andrea

      purple_blist_remove_chat (chat);
    }

    chatty_blist_chat_list_set_row ();

    chatty_window_change_view (CHATTY_VIEW_CHAT_LIST);
  }

  gtk_widget_destroy (dialog);
}


/**
 * chatty_blist_add_buddy:
 *
 * @account: a PurpleAccount
 *
 * Add a buddy to the chat list
 *
 */
void
chatty_blist_add_buddy (const char *who,
                        const char *whoalias)
{
  PurpleBuddy        *buddy;
  PurpleConversation *conv;
  PurpleBuddyIcon    *icon;

  chatty_data_t *chatty = chatty_get_data ();

  if (chatty->selected_account == NULL) {
    return;
  }

  if (*whoalias == '\0') {
    whoalias = NULL;
  }

  buddy = purple_buddy_new (chatty->selected_account, who, whoalias);

  purple_blist_add_buddy (buddy, NULL, NULL, NULL);

  g_debug ("chatty_blist_add_buddy: %s ", purple_buddy_get_name (buddy));

  purple_account_add_buddy_with_invite (chatty->selected_account, buddy, NULL);

  conv = purple_find_conversation_with_account (PURPLE_CONV_TYPE_IM,
                                                who,
                                                chatty->selected_account);

  if (conv != NULL) {
    icon = purple_conv_im_get_icon (PURPLE_CONV_IM(conv));

    if (icon != NULL) {
      purple_buddy_icon_update (icon);
    }
  }
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
 * chatty_blist_chat_list_select_first:
 *
 * Selectes the first chat in the chat list
 *
 * Called from cb_leaflet_notify_fold in
 * chatty-window.c
 */
void
chatty_blist_chat_list_select_first (void)
{
  GtkListBox *listbox = chatty_get_chats_list ();
  GtkListBoxRow *selected_row = gtk_list_box_get_selected_row (listbox);
  GtkListBoxRow *row = gtk_list_box_get_row_at_index (listbox, 0);

  if (selected_row != NULL)
    return;

  if (row != NULL)
    gtk_list_box_select_row (listbox, row);
}



/**
 * chatty_blist_contacts_remove_node:
 * @node:   a PurpleBlistNode
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

  if (!chatty_node || !chatty_node->row_chat) {
    return;
  }

  gtk_widget_destroy (GTK_WIDGET (chatty_node->row_chat));
  chatty_node->row_chat = NULL;
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
 * @autojoin:       a gboolean
 *
 * Filters the current row according to entry-text
 *
 */
void
chatty_blist_join_group_chat (PurpleAccount *account,
                              const char    *group_chat_id,
                              const char    *alias,
                              const char    *pwd,
                              gboolean       autojoin)
{
  PurpleChat               *chat;
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

  chat = purple_chat_new (account, group_chat_id, hash);

  if (chat != NULL) {
    purple_blist_add_chat (chat, NULL, NULL);
    purple_blist_alias_chat (chat, alias);
    purple_blist_node_set_bool ((PurpleBlistNode*)chat,
                                "chatty-autojoin",
                                autojoin);

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
  GtkListBox        *listbox;
  chatty_data_t     *chatty = chatty_get_data ();

  listbox = GTK_LIST_BOX (gtk_list_box_new ());

  chatty_get_data ()->listbox_chats = listbox;

  g_signal_connect (chatty->search_entry_chats,
                    "search-changed",
                    G_CALLBACK (cb_search_entry_changed),
                    listbox);

  gtk_list_box_set_filter_func (GTK_LIST_BOX (listbox), filter_chat_list_cb, chatty->search_entry_chats, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (listbox), chatty_blist_sort, NULL, NULL);

  g_signal_connect (listbox,
                    "row-selected",
                    G_CALLBACK (row_selected_cb),
                    NULL);

  gtk_box_pack_start (GTK_BOX (chatty->pane_view_chat_list), GTK_WIDGET(listbox), TRUE, TRUE, 0);
  gtk_widget_show_all (GTK_WIDGET(chatty->pane_view_chat_list));
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
  GtkListBox        *listbox;
  chatty_data_t     *chatty = chatty_get_data ();

  listbox = GTK_LIST_BOX (gtk_list_box_new ());

  chatty_get_data ()->listbox_contacts = listbox;

  g_signal_connect (chatty->search_entry_contacts,
                    "search-changed",
                    G_CALLBACK (cb_search_entry_changed),
                    listbox);

  gtk_list_box_set_filter_func (GTK_LIST_BOX (listbox), filter_chat_list_cb, chatty->search_entry_contacts, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (listbox), chatty_blist_sort, NULL, NULL);

  g_signal_connect (listbox,
                    "row-activated",
                    G_CALLBACK (row_selected_cb),
                    NULL);

  gtk_box_pack_start (GTK_BOX (chatty->pane_view_new_chat), GTK_WIDGET (listbox), TRUE, TRUE, 0);
  gtk_widget_show_all (GTK_WIDGET(chatty->pane_view_new_chat));
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
  const gchar   *alias;
  const gchar   *account_name;
  const gchar   *protocol_id;
  PurpleAccount *account;
  const char    *color;
  gboolean       blur;

  PurplePresence *presence = purple_buddy_get_presence (buddy);

  ChattyBlistNode *chatty_node = node->ui_data;

  account = purple_buddy_get_account (buddy);
  account_name = purple_account_get_username (account);

  if (!PURPLE_BLIST_NODE_IS_BUDDY (node)) {
    return;
  }

  // Do not add unknown contacts to the list
  if (purple_blist_node_get_bool (PURPLE_BLIST_NODE(buddy),
                                  "chatty-unknown-contact")) {
    return;
  }

  protocol_id = purple_account_get_protocol_id (account);

  alias = purple_buddy_get_alias (buddy);

  if (g_strcmp0 (protocol_id, "prpl-mm-sms") == 0) {
    color = CHATTY_COLOR_GREEN;
  } else {
    color = CHATTY_COLOR_BLUE;
  }

  if (purple_prefs_get_bool (CHATTY_PREFS_ROOT "/blist/greyout_offline_buddies") &&
      !PURPLE_BUDDY_IS_ONLINE(buddy)) {

    blur = TRUE;
  } else {
    blur = FALSE;
  }

  avatar = chatty_icon_get_buddy_icon ((PurpleBlistNode *)buddy,
                                       alias,
                                       CHATTY_ICON_SIZE_LARGE,
                                       color,
                                       blur);

  if (purple_prefs_get_bool (CHATTY_PREFS_ROOT "/blist/blur_idle_buddies") &&
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
                                                    NULL));

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
                                       CHATTY_ICON_SIZE_LARGE,
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
                                                    NULL));
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
  GtkListBox    *listbox;
  GdkPixbuf     *avatar;
  gchar         *name = NULL;
  const gchar   *tag;
  const gchar   *alias;
  const gchar   *protocol_id;
  gchar         *last_msg_text = NULL;
  gchar         *last_msg_ts = NULL;
  PurpleAccount *account;
  const char    *color;
  g_autofree gchar *unread_messages = NULL;
  gboolean notify;
  gboolean blur;

  PurplePresence *presence = purple_buddy_get_presence (buddy);

  ChattyBlistNode *chatty_node = node->ui_data;

  account = purple_buddy_get_account (buddy);

  if (!PURPLE_BLIST_NODE_IS_BUDDY (node)) {
    return;
  }

  protocol_id = purple_account_get_protocol_id (account);

  alias = purple_buddy_get_alias (buddy);

  if (g_strcmp0 (protocol_id, "prpl-mm-sms") == 0) {
    color = CHATTY_COLOR_GREEN;
  } else {
    color = CHATTY_COLOR_BLUE;
  }

  if (!purple_prefs_get_bool (CHATTY_PREFS_ROOT "/status/first_start")) {
     chatty_window_overlay_show (FALSE);
     purple_prefs_set_bool (CHATTY_PREFS_ROOT "/status/first_start", FALSE);
  }

  if (purple_prefs_get_bool (CHATTY_PREFS_ROOT "/blist/greyout_offline_buddies") &&
      !PURPLE_BUDDY_IS_ONLINE(buddy)) {

    blur = TRUE;
  } else {
    blur = FALSE;
  }

  avatar = chatty_icon_get_buddy_icon (node,
                                       alias,
                                       CHATTY_ICON_SIZE_LARGE,
                                       color,
                                       blur);

  if (purple_prefs_get_bool (CHATTY_PREFS_ROOT "/blist/blur_idle_buddies") &&
      purple_presence_is_idle (presence)) {

    chatty_icon_do_alphashift (avatar, 77);
  }

  if (chatty_node->conv.last_message_dir == MSG_IS_INCOMING) {
    tag = "";
  } else {
    tag = _("Me: ");
  }

  if (chatty_node->conv.last_message == NULL) {
    chatty_node->conv.last_message = "";
  }

  // FIXME: Don't hard code the color it should read it from the theme
  last_msg_text = g_markup_printf_escaped ("<span color='#3584e4'>%s</span><span alpha='55%%'>%s</span>",
                                           tag,
                                           chatty_node->conv.last_message);

  last_msg_ts = chatty_node->conv.last_msg_timestamp;

  if (purple_blist_node_get_bool (PURPLE_BLIST_NODE(buddy), "chatty-unknown-contact") &&
      purple_prefs_get_bool (CHATTY_PREFS_ROOT "/blist/indicate_unknown_contacts")) {
    name = g_markup_printf_escaped ("<span color='#FF3333'>%s</span>", alias);
  } else {
    name = g_markup_printf_escaped ("%s", alias);
  }

  notify = purple_blist_node_get_bool (node, "chatty-notifications");
  if (chatty_node->conv.pending_messages && notify) {
    unread_messages = g_strdup_printf ("%d", chatty_node->conv.pending_messages);
  }

  listbox = chatty_get_chats_list ();

  /* Create a new row or update the row if it already exists */
  if (chatty_node->row_chat == NULL) {
    chatty_node->row_chat = CHATTY_CONTACT_ROW (chatty_contact_row_new ((gpointer) node,
                                                    avatar,
                                                    name,
                                                    last_msg_text,
                                                    last_msg_ts,
                                                    unread_messages));
    gtk_widget_show (GTK_WIDGET (chatty_node->row_chat));
    gtk_container_add (GTK_CONTAINER (listbox), GTK_WIDGET (chatty_node->row_chat));
  } else {
    g_object_set (chatty_node->row_chat,
                  "avatar", avatar,
                  "name", name,
                  "description", last_msg_text,
                  "timestamp", last_msg_ts,
                  "message_count", unread_messages,
                  NULL);
  }

  gtk_list_box_invalidate_sort (listbox);

  if (avatar) {
    g_object_unref (avatar);
  }

  g_free (last_msg_text);
  g_free (last_msg_ts);
  g_free (name);
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
  GtkListBox    *listbox;
  PurpleChat    *chat;
  GdkPixbuf     *avatar = NULL;
  gchar         *name = NULL;
  const gchar   *chat_name;
  gchar         *last_msg_text = NULL;
  gchar         *last_msg_ts = NULL;
  g_autofree gchar *unread_messages = NULL;
  gboolean notify;

  ChattyBlistNode *chatty_node = node->ui_data;

  if (!PURPLE_BLIST_NODE_IS_CHAT (node)) {
    return;
  }

  chat = (PurpleChat*)node;

  if(!purple_account_is_connected (chat->account)) {
    return;
  }

  if (!purple_prefs_get_bool (CHATTY_PREFS_ROOT "/status/first_start")) {
     chatty_window_overlay_show (FALSE);
     purple_prefs_set_bool (CHATTY_PREFS_ROOT "/status/first_start", FALSE);
  }

  avatar = chatty_icon_get_buddy_icon (node,
                                       NULL,
                                       CHATTY_ICON_SIZE_LARGE,
                                       CHATTY_COLOR_BLUE,
                                       FALSE);

  chat_name = purple_chat_get_name (chat);

  if (chatty_node->conv.last_message == NULL) {
    chatty_node->conv.last_message = "";
  }

  // FIXME: Don't hard code the color it should read it from the theme
  last_msg_text = g_markup_printf_escaped ("<span color='#3584e4'>Group Chat: </span>%s",
                                           chatty_node->conv.last_message);

  last_msg_ts = chatty_node->conv.last_msg_timestamp;



  last_msg_ts = chatty_node->conv.last_msg_timestamp;

  notify = purple_blist_node_get_bool (node, "chatty-notifications");
  if (chatty_node->conv.pending_messages && notify) {
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
                                                    unread_messages));
    gtk_widget_show (GTK_WIDGET (chatty_node->row_chat));
    gtk_container_add (GTK_CONTAINER (listbox), GTK_WIDGET (chatty_node->row_chat));
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

  g_free (last_msg_text);
  g_free (last_msg_ts);
  g_free (name);
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
  const char              *who;
  g_autofree gchar        *iso_timestamp = NULL;
  struct tm               *timeinfo;
  char                     message_exists;

  g_return_if_fail (PURPLE_BLIST_NODE_IS_BUDDY(node));

  buddy = (PurpleBuddy*)node;

  account = purple_buddy_get_account (buddy);

  username = purple_account_get_username (account);
  who = purple_buddy_get_name (buddy);
  log_data = g_new0(ChattyLog, 1);

  message_exists = chatty_history_get_im_last_message(username, who, log_data);

  iso_timestamp = g_malloc0(MAX_GMT_ISO_SIZE * sizeof(char));

  if (purple_blist_node_get_bool (node, "chatty-autojoin") &&
      chatty_blist_buddy_is_displayable (buddy) &&
      message_exists) {


    timeinfo = localtime (&log_data->epoch);
    g_return_if_fail (strftime (iso_timestamp,
                                MAX_GMT_ISO_SIZE * sizeof(char),
                                "%I:%M %p",
                                timeinfo));

    ui = node->ui_data;
    ui->conv.last_message = log_data->msg;
    ui->conv.last_message_dir = log_data->dir;
    ui->conv.last_msg_ts_raw = log_data->epoch;
    ui->conv.last_msg_timestamp = g_steal_pointer (&iso_timestamp);

    chatty_blist_chats_update_node (buddy, node);
  } else {
    chatty_blist_chats_remove_node (node);
  }

  chatty_blist_contacts_update_node (buddy, node);
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
 * chatty_blist_new_list:
 * @blist: a PurpleBuddyList
 *
 * Creates a new PurpleBuddyList.
 * Function is called via PurpleBlistUiOps.
 *
 */
static void
chatty_blist_new_list (PurpleBuddyList *blist)
{
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
  chatty_blist_new_list,
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

  void *chatty_blist_handle = chatty_blist_get_handle();

  chatty_blist_create_chat_list ();
  chatty_blist_create_contact_list ();

  purple_prefs_add_none (CHATTY_PREFS_ROOT "/blist");
  purple_prefs_add_bool (CHATTY_PREFS_ROOT "/blist/show_buddy_icons", TRUE);
  purple_prefs_add_bool (CHATTY_PREFS_ROOT "/blist/show_idle_time", TRUE);
  purple_prefs_add_bool (CHATTY_PREFS_ROOT "/blist/show_offline_buddies", TRUE);
  purple_prefs_add_bool (CHATTY_PREFS_ROOT "/blist/greyout_offline_buddies", FALSE);
  purple_prefs_add_bool (CHATTY_PREFS_ROOT "/blist/blur_idle_buddies", FALSE);
  purple_prefs_add_bool (CHATTY_PREFS_ROOT "/blist/indicate_unknown_contacts", TRUE);
  purple_prefs_add_bool (CHATTY_PREFS_ROOT "/blist/show_protocol_icons", FALSE);

  purple_prefs_connect_callback (&handle,
                                 CHATTY_PREFS_ROOT "/blist/show_buddy_icons",
                                 cb_chatty_prefs_change_update_list,
                                 NULL);
  purple_prefs_connect_callback (&handle,
                                 CHATTY_PREFS_ROOT "/blist/show_idle_time",
                                 cb_chatty_prefs_change_update_list,
                                 NULL);
  purple_prefs_connect_callback (&handle,
                                 CHATTY_PREFS_ROOT "/blist/show_offline_buddies",
                                 cb_chatty_prefs_change_update_list,
                                 NULL);
  purple_prefs_connect_callback (&handle,
                                 CHATTY_PREFS_ROOT "/blist/indicate_unknown_contacts",
                                 cb_chatty_prefs_change_update_list,
                                 NULL);
  purple_prefs_connect_callback (&handle,
                                 CHATTY_PREFS_ROOT "/blist/show_protocol_icons",
                                 cb_chatty_prefs_change_update_list,
                                 NULL);
  purple_prefs_connect_callback (&handle,
                                 CHATTY_PREFS_ROOT "/blist/blur_idle_buddies",
                                 cb_chatty_prefs_change_update_list,
                                 NULL);
  purple_prefs_connect_callback (&handle,
                                 CHATTY_PREFS_ROOT "/blist/greyout_offline_buddies",
                                 cb_chatty_prefs_change_update_list,
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
chatty_blist_uninit (void) {
  purple_signals_unregister_by_instance (chatty_blist_get_handle());
  purple_signals_disconnect_by_handle (chatty_blist_get_handle());
}
