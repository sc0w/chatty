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
#include "chatty-buddy-list.h"
#include "chatty-conversation.h"
#define HANDY_USE_UNSTABLE_API
#include <handy.h>


static ChattyBuddyList *_chatty_blist = NULL;

static void chatty_blist_new_node (PurpleBlistNode *node);
static void chatty_blist_queue_refilter (GtkTreeModelFilter *filter);

static void chatty_blist_update (PurpleBuddyList *list,
                                 PurpleBlistNode *node);

static void chatty_blist_chats_remove_node (PurpleBuddyList *list,
                                            PurpleBlistNode *node,
                                            gboolean         update);

static void chatty_blist_contacts_remove_node (PurpleBuddyList *list,
                                               PurpleBlistNode *node,
                                               gboolean         update);

static void chatty_blist_update_buddy (PurpleBuddyList *list,
                                       PurpleBlistNode *node);

static void chatty_blist_draw_notification_badge (cairo_t *cr,
                                                  guint    row_y,
                                                  guint    row_height,
                                                  guint    num_msg);


// *** callbacks

static void
cb_tree_view_row_activated (GtkTreeView       *treeview,
                            GtkTreePath       *path,
                            GtkTreeViewColumn *column,
                            gpointer           user_data)
{
  PurpleAccount   *account;
  PurpleBlistNode *node;
  PurpleChat      *chat;
  const gchar     *protocol_id;
  GtkTreeModel    *treemodel;
  const char      *chat_name;
  const char      *buddy_alias;
  GtkTreeIter      iter;
  GdkPixbuf       *avatar;
  const char      *color;

  chatty_data_t *chatty = chatty_get_data ();

  treemodel = gtk_tree_view_get_model (treeview);

  gtk_tree_model_get_iter (GTK_TREE_MODEL(treemodel),
                           &iter,
                           path);

  gtk_tree_model_get (GTK_TREE_MODEL(treemodel),
                      &iter,
                      COLUMN_NODE,
                      &node,
                      -1);

  if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
    PurpleBuddy *buddy;

    buddy = (PurpleBuddy*)node;
    buddy_alias = purple_buddy_get_alias (buddy);
    account = purple_buddy_get_account (buddy);

    _chatty_blist->selected_node = node;

    protocol_id = purple_account_get_protocol_id (account);

    gtk_widget_hide (chatty->button_menu_chat_info);
    gtk_widget_hide (chatty->button_header_chat_info);



    if (purple_blist_node_get_bool (PURPLE_BLIST_NODE(buddy),
                                    "chatty-unknown-contact")) {

      gtk_widget_show (chatty->button_menu_add_contact);
      gtk_widget_show (chatty->separator_menu_msg_view);
    } else {
      gtk_widget_hide (chatty->button_menu_add_contact);
      gtk_widget_hide (chatty->separator_menu_msg_view);
    }

    if (g_strcmp0 (protocol_id, "prpl-mm-sms") == 0) {
      color = CHATTY_COLOR_GREEN;
    } else {
      color = CHATTY_COLOR_BLUE;
    }

    purple_blist_node_set_bool (node, "chatty-autojoin", TRUE);

    avatar = chatty_icon_get_buddy_icon (node,
                                         buddy_alias,
                                         CHATTY_ICON_SIZE_MEDIUM,
                                         color,
                                         FALSE);

    chatty_conv_im_with_buddy (account,
                               purple_buddy_get_name (buddy));

    chatty_window_update_sub_header_titlebar (avatar,
                                              buddy_alias);

    chatty_window_change_view (CHATTY_VIEW_MESSAGE_LIST);
    gtk_widget_hide (GTK_WIDGET(chatty->dialog_new_chat));

    g_object_unref (avatar);

  } else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
    chat = (PurpleChat*)node;
    chat_name = purple_chat_get_name (chat);

    _chatty_blist->selected_node = node;

    gtk_widget_show (chatty->button_menu_chat_info);
    gtk_widget_show (chatty->button_header_chat_info);
    gtk_widget_show (chatty->separator_menu_msg_view);
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
                         GtkTreeModelFilter *filter)
{
  chatty_blist_queue_refilter (filter);
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


static gboolean
cb_chatty_blist_refresh_timer (PurpleBuddyList *list)
{
  chatty_blist_refresh (purple_get_blist());

  return TRUE;
}


static void
cb_sign_on_off (PurpleConnection  *gc,
                PurpleBuddyList   *blist)
{
  // TODO ...
}


static void
cb_conversation_updated (PurpleConversation   *conv,
                         PurpleConvUpdateType  type,
                         ChattyBuddyList      *chatty_blist)
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
                          ChattyBuddyList     *chatty_blist)
{
  cb_conversation_updated (conv, PURPLE_CONV_UPDATE_UNSEEN, chatty_blist);
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

  if (ui->conv.conv != conv || !(flag & (PURPLE_MESSAGE_RECV))) {
    return;
  }

  if (node != _chatty_blist->selected_node) {
    ui->conv.flags |= CHATTY_BLIST_NODE_HAS_PENDING_MESSAGE;
    ui->conv.pending_messages ++;
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
                         ChattyBuddyList    *chatty_blist)
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
                ChattyBuddyList    *chatty_blist)
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
  chatty_blist_refresh (purple_get_blist ());
}


static gboolean
cb_notification_draw_badge (GtkWidget *widget,
                            cairo_t   *cr,
                            gpointer   user_data)
{
  PurpleBlistNode *node;
  GtkTreeIter      iter;
  GtkTreePath     *path;
  GdkRectangle     rect;
  ChattyBlistNode *chatty_node;
  gboolean         notify;

  if (!gtk_tree_model_iter_children (GTK_TREE_MODEL(_chatty_blist->treemodel_chats),
                                     &iter,
                                     NULL)) {
    return TRUE;
  }

  do {
    gtk_tree_model_get (GTK_TREE_MODEL(_chatty_blist->treemodel_chats),
                        &iter,
                        COLUMN_NODE,
                        &node,
                        -1);

    chatty_node = node->ui_data;

    path =
      gtk_tree_model_get_path (GTK_TREE_MODEL(_chatty_blist->treemodel_chats), &iter);

    gtk_tree_view_get_cell_area (_chatty_blist->treeview_chats,
                                 path,
                                 NULL,
                                 &rect);

    notify = purple_blist_node_get_bool (node, "chatty-notifications");

    if (chatty_node->conv.pending_messages && notify) {
      chatty_blist_draw_notification_badge (cr,
                                            rect.y,
                                            rect.height,
                                            chatty_node->conv.pending_messages);
    }

  } while (gtk_tree_model_iter_next (GTK_TREE_MODEL(_chatty_blist->treemodel_chats),
                                     &iter));

  return TRUE;
}


static gboolean
cb_auto_join_chats (gpointer data)
{
  PurpleBlistNode  *node;
  PurpleConnection *pc = data;
  PurpleAccount    *account = purple_connection_get_account (pc);

  for (node = purple_blist_get_root (); node;
       node = purple_blist_node_next (node, FALSE)) {

    if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
      PurpleChat *chat = (PurpleChat*)node;

      if (purple_chat_get_account (chat) == account &&
          purple_blist_node_get_bool (node, "chatty-autojoin")) {

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

/**
 * chatty_blist_draw_notification_badge:
 * @cr:         a cairo_t of the chat list
 * @row_y:      a guint
 * @row_height: a guint
 * @num_msg:    a guint
 *
 * Paints a badge with the number of unread
 * messages on a chat list entry
 */
static void
chatty_blist_draw_notification_badge (cairo_t *cr,
                                      guint    row_y,
                                      guint    row_height,
                                      guint    num_msg)
{
    GtkAllocation  *alloc;
    gchar          *num;
    int             x, x_offset;
    int             y, y_offset;
    int             width, height;
    double          rad, deg;

    alloc = g_new (GtkAllocation, 1);
    gtk_widget_get_allocation (GTK_WIDGET(_chatty_blist->treeview_chats), alloc);

    deg = M_PI / 180.0;
    rad = 10;
    width = 30;
    height = 20;

    if (num_msg > 9) {
      x =  alloc->width - 44;
      y = row_y + row_height - 34;
      y_offset = 15;

      if (num_msg > 99) {
        x_offset = -3;
      } else {
        x_offset = -7;
      }

      cairo_new_sub_path (cr);
      cairo_arc (cr, x + width - rad, y + rad, rad, -90 * deg, 0 * deg);
      cairo_arc (cr, x + width - rad, y + height - rad, rad, 0 * deg, 90 * deg);
      cairo_arc (cr, x + rad, y + height - rad, rad, 90 * deg, 180 * deg);
      cairo_arc (cr, x + rad, y + rad, rad, 180 * deg, 270 * deg);
      cairo_close_path (cr);
    } else {
      x =  alloc->width - 24;
      y = row_y + row_height - 24;
      x_offset = 4;
      y_offset = 5;

      cairo_arc (cr, x, y, 10, 0, 2 * M_PI);
    };

    cairo_set_source_rgb (cr, 0.29, 0.56, 0.85);
    cairo_fill (cr);

    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_set_font_size (cr, 14);

    num = g_strdup_printf ("%d", num_msg);

    cairo_select_font_face (cr,
                            "monospace",
                            CAIRO_FONT_SLANT_NORMAL,
                            CAIRO_FONT_WEIGHT_NORMAL);

    cairo_move_to (cr, x - x_offset, y + y_offset);
    cairo_show_text (cr, num);

    g_free(alloc);
    g_free (num);
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
 * chatty_blist_chat_list_selection_mode:
 *
 * @select: a gboolean
 *
 * Show selected list item when HdyLeaflet
 * is unfold
 *
 */
void
chatty_blist_chat_list_selection (gboolean select)
{
  GtkStyleContext *sc;

  if (_chatty_blist == NULL) {
    return;
  }

  sc = gtk_widget_get_style_context (GTK_WIDGET(_chatty_blist->treeview_chats));

  gtk_style_context_remove_class (sc, select ? "list_no_select" : "list_select");
  gtk_style_context_add_class (sc, select ? "list_select" : "list_no_select");
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
  PurpleBlistNode    *node;
  PurpleBuddy        *buddy;

  chatty_data_t *chatty = chatty_get_data ();

  node = _chatty_blist->selected_node;
  buddy = (PurpleBuddy*)node;

  conv = chatty_conv_container_get_active_purple_conv (GTK_NOTEBOOK(chatty->pane_view_message_list));

  if (buddy) {
    account = purple_conversation_get_account (conv);
    purple_account_add_buddy (account, buddy);
    purple_blist_node_remove_setting (PURPLE_BLIST_NODE(buddy), "chatty-unknown-contact");
  }
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

  node = _chatty_blist->selected_node;

  if (node) {
    purple_blist_node_set_bool (node, "chatty-autojoin", FALSE);
  }

  if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
    chatty_blist_chats_remove_node (purple_get_blist(), node, TRUE);
  }
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
  const char      *name;
  const char      *text;
  const char      *sub_text;
  int              response;

  chatty_data_t *chatty = chatty_get_data ();

  node = _chatty_blist->selected_node;

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

  dialog = gtk_message_dialog_new (chatty->main_window,
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
      chatty_conv_delete_message_history (buddy);

      purple_account_remove_buddy (buddy->account, buddy, NULL);
      purple_blist_remove_buddy (buddy);
      purple_conversation_destroy (ui->conv.conv);

      chatty_window_update_sub_header_titlebar (NULL, "");
    } else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
      purple_blist_remove_chat (chat);
    }

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
   _chatty_blist->selected_node = NULL;
}


/**
 * chatty_blist_refresh:
 * @list:  a PurpleBuddyList
 *
 * Refreshs the blist
 *
 */
void
chatty_blist_refresh (PurpleBuddyList *list)
{
  PurpleBlistNode *node;

  _chatty_blist = CHATTY_BLIST(list);

  if (!_chatty_blist || !_chatty_blist->treeview_chats) {
    return;
  }

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
 * chatty_blist_contacts_remove_node:
 * @list:   a PurpleBuddyList
 * @node:   a PurpleBlistNode
 * @update: a gboolean
 *
 * Removes a node in the contacts list
 *
 */
static void
chatty_blist_contacts_remove_node (PurpleBuddyList *list,
                                   PurpleBlistNode *node,
                                   gboolean         update)
{
  GtkTreeIter iter;
  GtkTreePath *path;

  ChattyBlistNode *chatty_node = node->ui_data;

  if (!chatty_node || !chatty_node->row_contact || !_chatty_blist) {
    return;
  }

  if (_chatty_blist->selected_node == node) {
    _chatty_blist->selected_node = NULL;
  }

  path = gtk_tree_row_reference_get_path (chatty_node->row_contact);

  if (path == NULL) {
    return;
  }

  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL(_chatty_blist->treemodel_contacts), &iter, path)) {
    gtk_tree_path_free (path);
    return;
  }

  gtk_list_store_remove (_chatty_blist->treemodel_contacts, &iter);

  gtk_tree_path_free (path);

  if (update && PURPLE_BLIST_NODE_IS_BUDDY(node)) {
    chatty_blist_update (list, node);
  }

  gtk_tree_row_reference_free (chatty_node->row_contact);
  chatty_node->row_contact = NULL;
}


/**
 * chatty_blist_chats_remove_node:
 * @list:   a PurpleBuddyList
 * @node:   a PurpleBlistNode
 * @update: a gboolean
 *
 * Removes a node in the chats list
 *
 */
static void
chatty_blist_chats_remove_node (PurpleBuddyList *list,
                                PurpleBlistNode *node,
                                gboolean         update)
{
  GtkTreeIter iter;
  GtkTreePath *path;

  ChattyBlistNode *chatty_node = node->ui_data;

  if (!chatty_node || !chatty_node->row_chat || !_chatty_blist) {
    return;
  }

  if (_chatty_blist->selected_node == node) {
    _chatty_blist->selected_node = NULL;
  }

  path = gtk_tree_row_reference_get_path (chatty_node->row_chat);

  if (path == NULL) {
    return;
  }

  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL(_chatty_blist->treemodel_chats), &iter, path)) {
    gtk_tree_path_free (path);
    return;
  }

  gtk_list_store_remove (_chatty_blist->treemodel_chats, &iter);

  gtk_tree_path_free (path);

  if (update) {
    chatty_blist_update (list, node);
  }

  gtk_tree_row_reference_free (chatty_node->row_chat);
  chatty_node->row_chat = NULL;
}


/**
 * chatty_blist_contact_list_add_columns:
 * @treeview: a GtkTreeView
 *
 * Setup columns contact list treeview.
 *
 */
static void
chatty_blist_contact_list_add_columns (GtkTreeView *treeview)
{
  GtkCellRenderer   *renderer;
  GtkTreeViewColumn *column;

  renderer = gtk_cell_renderer_pixbuf_new ();
  column = gtk_tree_view_column_new_with_attributes ("Avatar",
                                                     renderer,
                                                     "pixbuf",
                                                     COLUMN_AVATAR,
                                                     NULL);

  gtk_cell_renderer_set_padding (renderer, 12, 12);
  gtk_tree_view_append_column (treeview, column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Name",
                                                     renderer,
                                                     "text",
                                                     COLUMN_NAME,
                                                     NULL);

  gtk_tree_view_column_set_attributes (column, renderer,
                                       "markup", COLUMN_NAME,
                                       NULL);

  gtk_cell_renderer_set_alignment (renderer, 0.0, 0.2);
  gtk_tree_view_append_column (treeview, column);

  renderer = gtk_cell_renderer_pixbuf_new ();
  column = gtk_tree_view_column_new_with_attributes ("Icon",
                                                     renderer,
                                                     "pixbuf",
                                                     COLUMN_LAST,
                                                     NULL);


  g_object_set (renderer,
                "xalign", 0.95,
                "yalign", 0.2,
                NULL);

  gtk_tree_view_append_column (treeview, column);
}


/**
 * chatty_blist_chat_list_add_columns:
 * @treeview: a GtkTreeView
 *
 * Setup columns for chat list treeview.
 *
 */
static void
chatty_blist_chat_list_add_columns (GtkTreeView *treeview)
{
  GtkCellRenderer   *renderer;
  GtkTreeViewColumn *column;

  renderer = gtk_cell_renderer_pixbuf_new ();
  column = gtk_tree_view_column_new_with_attributes ("Avatar",
                                                     renderer,
                                                     "pixbuf",
                                                     COLUMN_AVATAR,
                                                     NULL);

  gtk_cell_renderer_set_padding (renderer, 12, 12);
  gtk_tree_view_append_column (treeview, column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Name",
                                                     renderer,
                                                     "text",
                                                     COLUMN_NAME,
                                                     NULL);

  gtk_tree_view_column_set_attributes (column, renderer,
                                       "markup", COLUMN_NAME,
                                       NULL);

  g_object_set (renderer,
                // TODO derive width-chars from screen width
                "width-chars", 24,
                "ellipsize", PANGO_ELLIPSIZE_END,
                NULL);

  gtk_cell_renderer_set_alignment (renderer, 0.0, 0.4);
  gtk_tree_view_append_column (treeview, column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Time",
                                                     renderer,
                                                     "text",
                                                     COLUMN_LAST,
                                                     NULL);

  gtk_tree_view_column_set_attributes (column,
                                       renderer,
                                       "markup", COLUMN_LAST,
                                       NULL);

  g_object_set (renderer,
                "xalign", 0.95,
                "yalign", 0.2,
                NULL);

  gtk_tree_view_append_column (treeview, column); //, FALSE);
}


static void *
chatty_blist_get_handle (void) {
  static int handle;

  return &handle;
}


/**
 * chatty_blist_do_refilter:
 * @filter:  a GtkTreeModelFilter
 *
 * Performs list filtering after filter_timeout
 *
 */
static gboolean
chatty_blist_do_refilter (GtkTreeModelFilter *filter)
{
  gtk_tree_model_filter_refilter (filter);

  _chatty_blist->filter_timeout = 0;

  return (FALSE);
}


/**
 * chatty_blist_queue_refilter:
 * @filter:  a GtkTreeModelFilter
 *
 * Prevent from filtering after every keypress
 * if query string is typed very quickly
 *
 */
static void
chatty_blist_queue_refilter (GtkTreeModelFilter *filter)
{
  if (_chatty_blist->filter_timeout) {
    g_source_remove (_chatty_blist->filter_timeout);
  }

  _chatty_blist->filter_timeout =
    g_timeout_add (300,
                   (GSourceFunc)chatty_blist_do_refilter,
                   filter);
}


/**
 * chatty_blist_entry_visible_func:
 * @model:  a GtkTreeModel
 " @iter:    a GtkTreeIter
 " @entry    a GtkEntry
 *
 * Filters the current row according to entry-text
 *
 */
static gboolean
chatty_blist_entry_visible_func (GtkTreeModel *model,
                                 GtkTreeIter  *iter,
                                 GtkEntry     *entry)
{
  const gchar *query;
  gchar       *str;
  gboolean     visible = FALSE;

  query = gtk_entry_get_text (entry);

  if (*query == '\0') {
    return( TRUE );
  }

  gtk_tree_model_get (model, iter, 2, &str, -1);

  if (str && strstr (str, query)) {
    visible = TRUE;
  }

  g_free (str);

  return visible;
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
chatty_blist_create_chat_list (PurpleBuddyList *list)
{
  GtkTreeView       *treeview;
  GtkTreeModel      *filter;
  GtkStyleContext   *sc;
  chatty_data_t     *chatty = chatty_get_data ();

  _chatty_blist = CHATTY_BLIST(list);
  _chatty_blist->filter_timeout = 0;
  _chatty_blist->treemodel_chats = gtk_list_store_new (NUM_COLUMNS,
                                                       G_TYPE_POINTER,
                                                       G_TYPE_OBJECT,
                                                       G_TYPE_STRING,
                                                       G_TYPE_STRING);

  filter = gtk_tree_model_filter_new (GTK_TREE_MODEL(_chatty_blist->treemodel_chats), NULL);

  g_signal_connect (chatty->search_entry_chats,
                    "search-changed",
                    G_CALLBACK (cb_search_entry_changed),
                    GTK_TREE_MODEL_FILTER(filter));

  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER(filter),
                                          (GtkTreeModelFilterVisibleFunc)chatty_blist_entry_visible_func,
                                          GTK_ENTRY(chatty->search_entry_chats),
                                          NULL);

  treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model (filter));
  gtk_tree_view_set_grid_lines (treeview, GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
  gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW(treeview), TRUE);
  sc = gtk_widget_get_style_context (GTK_WIDGET(treeview));
  gtk_style_context_add_class (sc, "list_no_select");
  g_signal_connect (treeview,
                    "row-activated",
                    G_CALLBACK (cb_tree_view_row_activated),
                    NULL);

  g_signal_connect_after (treeview,
                          "draw",
                          G_CALLBACK (cb_notification_draw_badge),
                          GINT_TO_POINTER (10));

  gtk_tree_view_set_headers_visible (treeview, FALSE);

  _chatty_blist->treeview_chats = treeview;

  gtk_widget_set_name (GTK_WIDGET(_chatty_blist->treeview_chats),
                       "chatty_blist_treeview");

  chatty_blist_chat_list_add_columns (GTK_TREE_VIEW (treeview));

  gtk_tree_view_columns_autosize (GTK_TREE_VIEW (treeview));

  gtk_box_pack_start (GTK_BOX (chatty->pane_view_chat_list),
                      GTK_WIDGET (_chatty_blist->treeview_chats),
                      TRUE, TRUE, 0);

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
chatty_blist_create_contact_list (PurpleBuddyList *list)
{
  GtkTreeView       *treeview;
  GtkTreeModel      *filter;
  GtkStyleContext   *sc;
  chatty_data_t     *chatty = chatty_get_data ();

  _chatty_blist = CHATTY_BLIST(list);
  _chatty_blist->filter_timeout = 0;
  _chatty_blist->treemodel_contacts = gtk_list_store_new (NUM_COLUMNS,
                                                          G_TYPE_POINTER,
                                                          G_TYPE_OBJECT,
                                                          G_TYPE_STRING,
                                                          G_TYPE_OBJECT);

  filter = gtk_tree_model_filter_new (GTK_TREE_MODEL(_chatty_blist->treemodel_contacts), NULL);

  g_signal_connect (chatty->search_entry_contacts,
                    "search-changed",
                    G_CALLBACK (cb_search_entry_changed),
                    GTK_TREE_MODEL_FILTER(filter));

  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER(filter),
                                          (GtkTreeModelFilterVisibleFunc)chatty_blist_entry_visible_func,
                                          GTK_ENTRY(chatty->search_entry_contacts),
                                          NULL);

  treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model (filter));
  gtk_tree_view_set_grid_lines (treeview, GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
  gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW(treeview), TRUE);
  sc = gtk_widget_get_style_context (GTK_WIDGET(treeview));
  gtk_style_context_add_class (sc, "list_no_select");
  g_signal_connect (treeview,
                    "row-activated",
                    G_CALLBACK (cb_tree_view_row_activated),
                    NULL);

  gtk_tree_view_set_headers_visible (treeview, FALSE);

  _chatty_blist->treeview_contacts = treeview;
  g_object_unref(G_OBJECT(filter));

  gtk_widget_set_name (GTK_WIDGET(_chatty_blist->treeview_contacts),
                       "chatty_blist_treeview");

  chatty_blist_contact_list_add_columns (GTK_TREE_VIEW (treeview));

  gtk_tree_view_columns_autosize (GTK_TREE_VIEW (treeview));

  gtk_box_pack_start (GTK_BOX (chatty->pane_view_new_chat),
                      GTK_WIDGET (_chatty_blist->treeview_contacts),
                      TRUE, TRUE, 0);

  gtk_widget_grab_focus (GTK_WIDGET(_chatty_blist->treeview_contacts));
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

  chatty_blist_create_chat_list (list);
  chatty_blist_create_contact_list (list);
  chatty_blist_refresh (list);

  purple_blist_set_visible (TRUE);

  // TODO deaktivate the timeout when the Phone is going idle
  // or when the Chatty UI hides
  _chatty_blist->refresh_timer =
    purple_timeout_add_seconds (30,
                                (GSourceFunc)cb_chatty_blist_refresh_timer,
                                list);

  handle = purple_connections_get_handle ();

  purple_signal_connect (handle, "signed-on", _chatty_blist,
                        PURPLE_CALLBACK(cb_sign_on_off), list);
  purple_signal_connect (handle, "signed-off", _chatty_blist,
                        PURPLE_CALLBACK(cb_sign_on_off), list);

  handle = purple_conversations_get_handle();

  purple_signal_connect (handle, "conversation-updated", _chatty_blist,
                         PURPLE_CALLBACK(cb_conversation_updated),
                         _chatty_blist);
  purple_signal_connect (handle, "deleting-conversation", _chatty_blist,
                         PURPLE_CALLBACK(cb_conversation_deleting),
                         _chatty_blist);
  purple_signal_connect (handle, "conversation-created", _chatty_blist,
                         PURPLE_CALLBACK(cb_conversation_created),
                         _chatty_blist);
  purple_signal_connect (handle,
                         "chat-joined",
                         _chatty_blist,
                         PURPLE_CALLBACK(cb_chat_joined),
                         _chatty_blist);

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

  chatty_blist_chats_remove_node (list, node, TRUE);
  chatty_blist_contacts_remove_node (list, node, TRUE);

  if (chatty_node) {
    if (chatty_node->recent_signonoff_timer > 0) {
      purple_timeout_remove(chatty_node->recent_signonoff_timer);
    }

    purple_signals_disconnect_by_handle (node->ui_data);

    g_free (node->ui_data);
    node->ui_data = NULL;
  }
}


/**
 * chatty_blist_chats_sort:
 * @node:     a PurpleBlistNode
 * @cur_iter: a PurpleBuddy
 * @iter:     a PurpleBlistNode
 *
 * Sorts the chats-list based on
 * purple_log_get_activity_score
 *
 */
static void
chatty_blist_chats_sort (PurpleBlistNode *node,
                         GtkTreeIter     *cur_iter,
                         GtkTreeIter     *iter)
{
  GtkTreeIter more_z;

  int activity_score = 0, this_log_activity_score = 0;
  const char *buddy_name, *this_buddy_name;

  if(cur_iter && (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(_chatty_blist->treemodel_chats), NULL) == 1)) {
    *iter = *cur_iter;
    return;
  }

  if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
    PurpleBuddy *buddy;

    buddy = (PurpleBuddy*)node;
    activity_score = purple_log_get_activity_score (PURPLE_LOG_IM,
                                                    buddy->name,
                                                    buddy->account);

    buddy_name = purple_buddy_get_alias ((PurpleBuddy*)node);
  }

  if (!gtk_tree_model_iter_children (GTK_TREE_MODEL(_chatty_blist->treemodel_chats),
                                     &more_z,
                                     NULL)) {

    gtk_list_store_insert (_chatty_blist->treemodel_chats, iter, 0);

    return;
  }

  do {
    PurpleBlistNode *n;
    PurpleBuddy     *buddy;
    int              cmp;

    gtk_tree_model_get (GTK_TREE_MODEL(_chatty_blist->treemodel_chats),
                        &more_z,
                        COLUMN_NODE,
                        &n,
                        -1);

    this_log_activity_score = 0;

    if(PURPLE_BLIST_NODE_IS_BUDDY(n)) {
      buddy = (PurpleBuddy*)n;

      this_log_activity_score += purple_log_get_activity_score (PURPLE_LOG_IM,
                                                                buddy->name,
                                                                buddy->account);

      this_buddy_name = purple_buddy_get_alias ((PurpleBuddy*)n);
    } else {
      this_buddy_name = NULL;
    }

    cmp = purple_utf8_strcasecmp (buddy_name, this_buddy_name);

    if (!PURPLE_BLIST_NODE_IS_BUDDY(n) || activity_score > this_log_activity_score ||
        ((activity_score == this_log_activity_score) &&
         (cmp < 0 || (cmp == 0 && node < n)))) {

      if (cur_iter != NULL) {
        gtk_list_store_move_before (_chatty_blist->treemodel_chats, cur_iter, &more_z);
        *iter = *cur_iter;
        return;
      }
    }
  } while (gtk_tree_model_iter_next (GTK_TREE_MODEL(_chatty_blist->treemodel_chats),
                                     &more_z));

  if (cur_iter != NULL) {
    gtk_list_store_move_before (_chatty_blist->treemodel_chats, cur_iter, NULL);
    *iter = *cur_iter;
    return;
  } else {
    gtk_list_store_append (_chatty_blist->treemodel_chats, iter);
    return;
  }
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
  GtkTreeIter    iter;
  GdkPixbuf     *avatar;
  GtkTreePath   *path;
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

  name = g_strconcat ("<span color='#646464'>",
                      alias,
                      "</span>",
                      "\n",
                      "<span color='darkgrey'>",
                      account_name,
                      "</span>",
                      NULL);

  if (!chatty_node->row_contact) {
    gtk_list_store_append (_chatty_blist->treemodel_contacts, &iter);

    path =
      gtk_tree_model_get_path (GTK_TREE_MODEL(_chatty_blist->treemodel_contacts), &iter);
    chatty_node->row_contact =
      gtk_tree_row_reference_new (GTK_TREE_MODEL(_chatty_blist->treemodel_contacts), path);
  } else {
    path = gtk_tree_row_reference_get_path (chatty_node->row_contact);

    if (path != NULL) {
      gtk_tree_model_get_iter (GTK_TREE_MODEL(_chatty_blist->treemodel_contacts), &iter, path);
    }
  }

  if (path != NULL) {
    gtk_list_store_set (_chatty_blist->treemodel_contacts, &iter,
                        COLUMN_NODE, node,
                        COLUMN_AVATAR, avatar,
                        COLUMN_NAME, name,
                        -1);
  }

  if (avatar) {
    g_object_unref (avatar);
  }

  gtk_tree_path_free (path);
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
  GtkTreeIter    iter;
  GdkPixbuf     *avatar;
  GtkTreePath   *path;
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

  name = g_strconcat ("<span color='#646464'>",
                      chat_name,
                      "</span>",
                      "\n",
                      "<span color='darkgrey'>",
                      account_name,
                      "</span>",
                      NULL);

  if (!chatty_node->row_contact) {
    gtk_list_store_append (_chatty_blist->treemodel_contacts, &iter);

    path =
      gtk_tree_model_get_path (GTK_TREE_MODEL(_chatty_blist->treemodel_contacts), &iter);
    chatty_node->row_contact =
      gtk_tree_row_reference_new (GTK_TREE_MODEL(_chatty_blist->treemodel_contacts), path);
  } else {
    path = gtk_tree_row_reference_get_path (chatty_node->row_contact);

    if (path != NULL) {
      gtk_tree_model_get_iter (GTK_TREE_MODEL(_chatty_blist->treemodel_contacts), &iter, path);
    }
  }

  if (path != NULL) {
    gtk_list_store_set (_chatty_blist->treemodel_contacts, &iter,
                        COLUMN_NODE, node,
                        COLUMN_AVATAR, avatar,
                        COLUMN_NAME, name,
                        -1);
  }

  if (avatar) {
    g_object_unref (avatar);
  }

  gtk_tree_path_free (path);
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
  GtkTreeIter    iter;
  GtkTreeIter    cur_iter;
  GdkPixbuf     *avatar;
  GtkTreePath   *path;
  gchar         *name = NULL;
  const gchar   *tag;
  const gchar   *alias;
  const gchar   *b_name;
  const gchar   *protocol_id;
  gchar         *last_msg_text = NULL;
  gchar         *last_msg_ts = NULL;
  PurpleAccount *account;
  const char    *color;
  const char    *color_tag;
  gboolean       blur;

  PurplePresence *presence = purple_buddy_get_presence (buddy);

  ChattyBlistNode *chatty_node = node->ui_data;

  account = purple_buddy_get_account (buddy);

  if (!PURPLE_BLIST_NODE_IS_BUDDY (node)) {
    return;
  }

  protocol_id = purple_account_get_protocol_id (account);

  b_name = purple_buddy_get_name (buddy);
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

  avatar = chatty_icon_get_buddy_icon (node,
                                       alias,
                                       CHATTY_ICON_SIZE_LARGE,
                                       color,
                                       blur);

  if (purple_prefs_get_bool (CHATTY_PREFS_ROOT "/blist/blur_idle_buddies") &&
      purple_presence_is_idle (presence)) {

    chatty_icon_do_alphashift (avatar, 77);
  }

  if ((g_strcmp0 (chatty_node->conv.last_message_name, b_name)) == 0) {
    tag = "";
  } else {
    tag = _("Me: ");
  }

  if (chatty_node->conv.last_message == NULL) {
    chatty_node->conv.last_message = "";
  }

  last_msg_text = g_strconcat ("<span color='#c0c0c0'>",
                               tag,
                               "</span>",
                               "<span color='#646464'>",
                               chatty_node->conv.last_message,
                               "</span>",
                               NULL);

  last_msg_ts = g_strconcat ("<span color='#646464'>",
                             "<small>",
                             chatty_node->conv.last_msg_timestamp,
                             "</small>"
                             "</span>",
                             NULL);

  if (purple_blist_node_get_bool (PURPLE_BLIST_NODE(buddy), "chatty-unknown-contact") &&
      purple_prefs_get_bool (CHATTY_PREFS_ROOT "/blist/indicate_unknown_contacts")) {

    color_tag = "<span color='#FF3333'>";
  } else {
    color_tag = "<span color='#646464'>";
  }

  if (chatty_node->conv.flags & CHATTY_BLIST_NODE_HAS_PENDING_MESSAGE) {
    name = g_strconcat (color_tag,
                        alias,
                        "</span>",
                        "\n",
                        "<small>",
                        last_msg_text,
                        "</small>",
                        NULL);
  } else {
    name = g_strconcat (color_tag,
                        alias,
                        "\n",
                        "<small>",
                        last_msg_text,
                        "</small>",
                        "</span>",
                        NULL);
  }

  if (!chatty_node->row_chat) {
    gtk_list_store_append (_chatty_blist->treemodel_chats, &iter);
  } else {
    path = gtk_tree_row_reference_get_path (chatty_node->row_chat);

    if (path != NULL) {
      gtk_tree_model_get_iter (GTK_TREE_MODEL(_chatty_blist->treemodel_chats), &cur_iter, path);
      chatty_blist_chats_sort (node, &cur_iter, &iter);
    }
  }

  path = gtk_tree_model_get_path (GTK_TREE_MODEL(_chatty_blist->treemodel_chats), &iter);
  chatty_node->row_chat = gtk_tree_row_reference_new (GTK_TREE_MODEL(_chatty_blist->treemodel_chats), path);

  if (path != NULL) {
    gtk_list_store_set (_chatty_blist->treemodel_chats, &iter,
                        COLUMN_NODE, node,
                        COLUMN_AVATAR, avatar,
                        COLUMN_NAME, name,
                        COLUMN_LAST, last_msg_ts,
                        -1);
  }

  if (avatar) {
    g_object_unref (avatar);
  }

  gtk_tree_path_free (path);
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
  GtkTreeIter    iter;
  GtkTreeIter    cur_iter;
  PurpleChat    *chat;
  GdkPixbuf     *avatar = NULL;
  GtkTreePath   *path;
  gchar         *name = NULL;
  const gchar   *chat_name;
  gchar         *last_msg_text = NULL;
  gchar         *last_msg_ts = NULL;

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

  chat_name = purple_chat_get_name (chat);


  if (chatty_node->conv.last_message == NULL) {
    chatty_node->conv.last_message = "";
  }

  last_msg_text = g_strconcat ("<span color='#c0c0c0'>",
                               "Group Chat",
                               "</span>",
                               "<span color='#646464'>",
                               chatty_node->conv.last_message,
                               "</span>",
                               NULL);

  last_msg_ts = g_strconcat ("<span color='#646464'>",
                             "<small>",
                             chatty_node->conv.last_msg_timestamp,
                             "</small>"
                             "</span>",
                             NULL);

  if (chatty_node->conv.flags & CHATTY_BLIST_NODE_HAS_PENDING_MESSAGE) {
    name = g_strconcat ("<span color='#646464'>",
                        chat_name,
                        "</span>",
                        "\n",
                        "<small>",
                        last_msg_text,
                        "</small>",
                        NULL);
  } else {
    name = g_strconcat ("<span color='#646464'>",
                        chat_name,
                        "\n",
                        "<small>",
                        last_msg_text,
                        "</small>",
                        "</span>",
                        NULL);
  }

  if (!chatty_node->row_chat) {
    gtk_list_store_append (_chatty_blist->treemodel_chats, &iter);
  } else {
    path = gtk_tree_row_reference_get_path (chatty_node->row_chat);

    if (path != NULL) {
      gtk_tree_model_get_iter (GTK_TREE_MODEL(_chatty_blist->treemodel_chats), &cur_iter, path);
      chatty_blist_chats_sort (node, &cur_iter, &iter);
    }
  }

  path = gtk_tree_model_get_path (GTK_TREE_MODEL(_chatty_blist->treemodel_chats), &iter);
  chatty_node->row_chat = gtk_tree_row_reference_new (GTK_TREE_MODEL(_chatty_blist->treemodel_chats), path);

  if (path != NULL) {
    gtk_list_store_set (_chatty_blist->treemodel_chats, &iter,
                        COLUMN_NODE, node,
                        COLUMN_AVATAR, avatar,
                        COLUMN_NAME, name,
                        COLUMN_LAST, NULL,
                        -1);
  }

  if (avatar) {
    g_object_unref (avatar);
  }

  gtk_tree_path_free (path);
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
  PurpleBuddy     *buddy;
  ChattyLog       *log_data = NULL;
  ChattyBlistNode *ui;

  g_return_if_fail (PURPLE_BLIST_NODE_IS_BUDDY(node));

  buddy = (PurpleBuddy*)node;

  log_data = chatty_conv_message_get_last_msg (buddy);

  if (purple_blist_node_get_bool (node, "chatty-autojoin") &&
      chatty_blist_buddy_is_displayable (buddy) &&
      log_data != NULL) {

    ui = node->ui_data;
    ui->conv.last_message = log_data->msg;
    ui->conv.last_message_name = log_data->name;
    ui->conv.last_msg_timestamp = log_data->time_stamp;

    chatty_blist_chats_update_node (buddy, node);
  } else {
    chatty_blist_chats_remove_node (list, node, FALSE);
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
  if (list) {
    _chatty_blist = CHATTY_BLIST(list);
  }

  if (!_chatty_blist || !_chatty_blist->treeview_chats || !node) {
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
  if (!list || !list->ui_data) {
    return;
  }

  g_return_if_fail (list->ui_data == _chatty_blist);

  purple_signals_disconnect_by_handle (_chatty_blist);

  gtk_widget_destroy (GTK_WIDGET(_chatty_blist->box));

  if (_chatty_blist->refresh_timer) {
    purple_timeout_remove (_chatty_blist->refresh_timer);
  }

  _chatty_blist->refresh_timer = 0;
  _chatty_blist->box = NULL;
  _chatty_blist->treeview_chats = NULL;
  g_object_unref (G_OBJECT(_chatty_blist->treemodel_chats));
  _chatty_blist->treemodel_chats = NULL;

  g_free (_chatty_blist);

  _chatty_blist = NULL;
  purple_prefs_disconnect_by_handle (chatty_blist_get_handle ());
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
  PurpleBuddy *buddy;
  const char  *account_name;

  buddy = purple_find_buddy (account, username);

  if (buddy == NULL) {
    buddy = purple_buddy_new (account, username, alias);

    purple_blist_add_buddy (buddy, NULL, NULL, NULL);
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
  ChattyBuddyList *chatty_blist;

  chatty_blist = g_new0 (ChattyBuddyList, 1);

  blist->ui_data = chatty_blist;
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

  void *chatty_blist_handle = chatty_blist_get_handle();

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
}


void
chatty_blist_uninit (void) {
  purple_signals_unregister_by_instance (chatty_blist_get_handle());
  purple_signals_disconnect_by_handle (chatty_blist_get_handle());
}
