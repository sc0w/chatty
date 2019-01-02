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

static void chatty_blist_chats_hide_node (PurpleBuddyList *list,
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
  const gchar     *protocol_id;
  GtkTreeModel    *treemodel;
  PurpleBlistNode *node;
  GtkTreeIter      iter;
  GdkPixbuf       *avatar;
  guint            color;

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
    account = purple_buddy_get_account (buddy);

    _chatty_blist->selected_node = node;

    protocol_id = purple_account_get_protocol_id (account);

    if (g_strcmp0 (protocol_id, "prpl-mm-sms") == 0) {
      color = CHATTY_ICON_COLOR_GREEN;
    } else {
      color = CHATTY_ICON_COLOR_BLUE;
    }

    avatar = chatty_icon_get_buddy_icon ((PurpleBlistNode *)buddy,
                                         CHATTY_ICON_SIZE_MEDIUM,
                                         color,
                                         FALSE);

    chatty_conv_im_with_buddy (account,
                               purple_buddy_get_name (buddy));

    chatty_window_set_header_title (purple_buddy_get_name (buddy));
    chatty_window_change_view (CHATTY_VIEW_MESSAGE_LIST);

    if (avatar != NULL) {
      gtk_image_set_from_pixbuf (GTK_IMAGE(chatty->header_icon), avatar);
      g_object_unref (avatar);
    } else {
      gtk_image_clear (GTK_IMAGE(chatty->header_icon));
    }
  }
}


static void
cb_search_entry_changed (GtkSearchEntry     *entry,
                         GtkTreeModelFilter *filter)
{
  chatty_blist_queue_refilter (filter);
}


static void
cb_button_new_conversation_clicked (GtkButton *sender,
                                    gpointer   account)
{
  chatty_data_t *chatty = chatty_get_data ();

  chatty_blist_add_buddy (account);
  chatty_window_change_view (CHATTY_VIEW_CHAT_LIST);

  gtk_container_foreach (GTK_CONTAINER(chatty->pane_view_new_contact),
                         (GtkCallback)gtk_widget_destroy, NULL);
}


static void
cb_buddy_name_insert_text (GtkEntry    *entry,
                           const gchar *text,
                           gint         length,
                           gint        *position,
                           gpointer     data)
{
  chatty_data_t *chatty = chatty_get_data ();

  // TODO validate input
  if (length) {
    gtk_widget_set_sensitive (chatty->button_add_buddy, TRUE);
  } else {
    gtk_widget_set_sensitive (chatty->button_add_buddy, FALSE);
  }
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

  chatty_blist_update_buddy (purple_get_blist (), PURPLE_BLIST_NODE(buddy));
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

  // TODO set the status in the message list popover

  g_debug ("Buddy \"%s\"\n (%s) signed on/off", purple_buddy_get_name (buddy),
           purple_account_get_protocol_id (purple_buddy_get_account(buddy)));
}


static gboolean
cb_chatty_blist_refresh_timer (PurpleBuddyList *list)
{
  chatty_blist_refresh (purple_get_blist(), FALSE);

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
      chatty_blist_update_buddy (NULL, (PurpleBlistNode *)buddy);
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

  if (ui->conv.conv != chatty_conv->active_conv) {
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
cb_chatty_prefs_change_update_list (const char     *name,
                                    PurplePrefType  type,
                                    gconstpointer   val,
                                    gpointer        data)
{
  chatty_blist_refresh (purple_get_blist (), FALSE);
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

    if (chatty_node->conv.pending_messages) {
      chatty_blist_draw_notification_badge (cr,
                                            rect.y,
                                            rect.height,
                                            chatty_node->conv.pending_messages);
    }

  } while (gtk_tree_model_iter_next (GTK_TREE_MODEL(_chatty_blist->treemodel_chats),
                                     &iter));

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
  GtkWidget       *dialog;
  const char      *buddy_name;
  int              response;

  chatty_data_t *chatty = chatty_get_data ();

  node = _chatty_blist->selected_node;
  buddy = (PurpleBuddy*)node;

  buddy_name = purple_buddy_get_alias (buddy);

  dialog = gtk_message_dialog_new (chatty->main_window,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("Delete Chat with %s?"),
                                   buddy_name);

  gtk_dialog_add_buttons (GTK_DIALOG(dialog),
                          _("Cancel"),
                          GTK_RESPONSE_CANCEL,
                          _("Delete"),
                          GTK_RESPONSE_OK,
                          NULL);

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG(dialog),
                                            _("This deletes the conversation history"));

  gtk_dialog_set_default_response (GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
  gtk_window_set_position (GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

  response = gtk_dialog_run (GTK_DIALOG(dialog));

  if (response == GTK_RESPONSE_OK) {
    ui = node->ui_data;

    chatty_conv_delete_message_history (buddy);
    purple_conversation_destroy (ui->conv.conv);
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
chatty_blist_add_buddy (PurpleAccount *account)
{
  const gchar        *who, *whoalias;
  PurpleBuddy        *buddy;
  PurpleConversation *conv;
  PurpleBuddyIcon    *icon;

  chatty_data_t *chatty = chatty_get_data ();

  who = gtk_entry_get_text (GTK_ENTRY(chatty->entry_buddy_name));
  whoalias = gtk_entry_get_text (GTK_ENTRY(chatty->entry_buddy_nick));

  if (*whoalias == '\0') {
    whoalias = NULL;
  }

  buddy = purple_buddy_new (account, who, whoalias);

  purple_blist_add_buddy (buddy, NULL, NULL, NULL);

  g_debug ("chatty_blist_add_buddy: %s ", purple_buddy_get_name (buddy));

  purple_account_add_buddy_with_invite (account, buddy, NULL);

  conv = purple_find_conversation_with_account (PURPLE_CONV_TYPE_IM,
                                                who,
                                                account);

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
 * chatty_blist_create_add_buddy_view:
 *
 * Creates a view to add a new contact by its ID
 *
 * Called from cb_list_account_select_row_activated in
 * chatty-account.c
 */
void
chatty_blist_create_add_buddy_view (PurpleAccount *account)
{
// TODO create this view in a *.ui file for interface builder
  GtkWidget   *grid;
  GtkWidget   *label;
  GtkWidget   *button_avatar;

  chatty_data_t *chatty = chatty_get_data ();

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 30);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 10);

  button_avatar = chatty_icon_get_avatar_button (80);

  gtk_grid_attach (GTK_GRID (grid),
                   GTK_WIDGET (button_avatar),
                   0, 0, 2, 1);

  chatty->label_buddy_id = gtk_label_new (NULL);

  gtk_grid_attach (GTK_GRID (grid),
                   GTK_WIDGET (chatty->label_buddy_id),
                   0, 1, 1, 1);

  chatty->entry_buddy_name = GTK_ENTRY (gtk_entry_new ());

  g_signal_connect (G_OBJECT(chatty->entry_buddy_name),
                    "insert_text",
                    G_CALLBACK(cb_buddy_name_insert_text),
                    NULL);

  gtk_grid_attach (GTK_GRID (grid),
                   GTK_WIDGET (chatty->entry_buddy_name),
                   1, 1, 1, 1);

  label = gtk_label_new ("Name");

  gtk_grid_attach (GTK_GRID (grid),
                   GTK_WIDGET (label),
                   0, 2, 1, 1);

  chatty->entry_buddy_nick = GTK_ENTRY (gtk_entry_new ());
  gtk_grid_attach (GTK_GRID (grid),
                   GTK_WIDGET (chatty->entry_buddy_nick),
                   1, 2, 1, 1);

  chatty->button_add_buddy = gtk_button_new_with_label ("Add");
  gtk_grid_attach (GTK_GRID (grid),
                   GTK_WIDGET (chatty->button_add_buddy),
                   1, 3, 1, 1);

  gtk_widget_set_sensitive (GTK_WIDGET (chatty->button_add_buddy), FALSE);
  g_signal_connect (chatty->button_add_buddy,
                    "clicked",
                    G_CALLBACK (cb_button_new_conversation_clicked),
                    (gpointer) account);

  gtk_widget_set_halign (GTK_WIDGET (grid), GTK_ALIGN_CENTER);
  gtk_widget_set_valign (GTK_WIDGET (grid), GTK_ALIGN_CENTER);

  gtk_widget_show_all (grid);

  gtk_widget_set_can_focus (GTK_WIDGET(chatty->entry_buddy_name), TRUE);
  gtk_widget_grab_focus (GTK_WIDGET(chatty->entry_buddy_name));

  gtk_box_pack_start (GTK_BOX (chatty->pane_view_new_contact),
                      grid, TRUE, TRUE, 0);
}


/**
 * chatty_blist_refresh:
 * @list:  a PurpleBuddyList
 *
 * Refreshs the blist
 *
 */
void
chatty_blist_refresh (PurpleBuddyList *list,
                      gboolean         remove)
{
  PurpleBlistNode *node;

  _chatty_blist = CHATTY_BLIST(list);

  if (!_chatty_blist                 ||
      !_chatty_blist->treeview_chats ||
      !_chatty_blist->treeview_chats) {

    return;
  }

  node = list->root;

  while (node)
  {
    if (remove && !PURPLE_BLIST_NODE_IS_GROUP(node)) {
      chatty_blist_chats_hide_node (list, node, FALSE);
    }

    if (PURPLE_BLIST_NODE_IS_BUDDY (node)) {
      chatty_blist_update_buddy (list, node);
    } else if (PURPLE_BLIST_NODE_IS_GROUP (node)) {
      chatty_blist_update (list, node);
    }

    node = purple_blist_node_next (node, FALSE);
  }
}


/**
 * chatty_blist_chats_hide_node:
 * @list:   a PurpleBuddyList
 * @node:   a PurpleBlistNode
 * @update: a gboolean
 *
 * Hides a node in the blist
 *
 */
static void
chatty_blist_chats_hide_node (PurpleBuddyList *list,
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

  if(update && PURPLE_BLIST_NODE_IS_BUDDY(node)) {
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
                "width-chars", 34,
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
  GtkWidget         *vbox;
  HdyColumn         *hdy_column;
  GtkWidget         *search_entry;
  GtkStyleContext   *sc;
  chatty_data_t     *chatty = chatty_get_data ();

  _chatty_blist = CHATTY_BLIST(list);
  _chatty_blist->filter_timeout = 0;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  chatty->search_bar = hdy_search_bar_new ();
  g_object_set  (G_OBJECT(chatty->search_bar),
                 "hexpand", TRUE,
                 "halign",  GTK_ALIGN_FILL,
                 NULL);

  search_entry = gtk_search_entry_new ();
  g_object_set  (G_OBJECT(search_entry),
                 "hexpand", TRUE,
                 NULL);
  hdy_search_bar_set_show_close_button (HDY_SEARCH_BAR (chatty->search_bar), FALSE);
  hdy_search_bar_connect_entry (HDY_SEARCH_BAR (chatty->search_bar),
                                GTK_ENTRY (search_entry));

  hdy_column = hdy_column_new ();
  g_object_set (G_OBJECT(hdy_column),
                "maximum-width", 600,
                "hexpand", TRUE,
                NULL);

  gtk_container_add (GTK_CONTAINER(hdy_column), GTK_WIDGET(search_entry));
  gtk_container_add (GTK_CONTAINER (chatty->search_bar), GTK_WIDGET(hdy_column));
  gtk_box_pack_start (GTK_BOX (vbox),
                      GTK_WIDGET (chatty->search_bar),
                      FALSE, FALSE, 0);

  _chatty_blist->treemodel_chats = gtk_list_store_new (NUM_COLUMNS,
                                                       G_TYPE_POINTER,
                                                       G_TYPE_OBJECT,
                                                       G_TYPE_STRING,
                                                       G_TYPE_STRING);

  filter = gtk_tree_model_filter_new (GTK_TREE_MODEL(_chatty_blist->treemodel_chats), NULL);

  g_signal_connect (search_entry,
                    "search-changed",
                    G_CALLBACK (cb_search_entry_changed),
                    GTK_TREE_MODEL_FILTER(filter));

  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER(filter),
                                          (GtkTreeModelFilterVisibleFunc)chatty_blist_entry_visible_func,
                                          GTK_ENTRY(search_entry),
                                          NULL);

  treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model (filter));
  gtk_tree_view_set_grid_lines (treeview, GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
  gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW(treeview), TRUE);
  sc = gtk_widget_get_style_context (GTK_WIDGET(treeview));
  gtk_style_context_add_class (sc, "buddy_list");
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

  gtk_box_pack_start (GTK_BOX (vbox),
                      GTK_WIDGET(_chatty_blist->treeview_chats),
                      TRUE, TRUE, 0);

  gtk_box_pack_start (GTK_BOX (chatty->pane_view_chat_list),
                      GTK_WIDGET (vbox),
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
  GtkWidget         *vbox;
  HdyColumn         *hdy_column;
  GtkWidget         *search_bar;
  GtkWidget         *search_entry;
  GtkStyleContext   *sc;
  chatty_data_t     *chatty = chatty_get_data ();

  _chatty_blist = CHATTY_BLIST(list);

  _chatty_blist->filter_timeout = 0;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  search_bar = hdy_search_bar_new ();
  g_object_set  (G_OBJECT(search_bar),
                 "hexpand", TRUE,
                 "halign",  GTK_ALIGN_FILL,
                 NULL);

  search_entry = gtk_search_entry_new ();
  g_object_set  (G_OBJECT(search_entry),
                 "hexpand", TRUE,
                 NULL);
  hdy_search_bar_set_show_close_button (HDY_SEARCH_BAR (search_bar), FALSE);
  hdy_search_bar_set_search_mode (HDY_SEARCH_BAR (search_bar), TRUE);
  hdy_search_bar_connect_entry (HDY_SEARCH_BAR (search_bar),
                                GTK_ENTRY (search_entry));

  hdy_column = hdy_column_new ();
  g_object_set (G_OBJECT(hdy_column),
                "maximum-width", 600,
                "hexpand", TRUE,
                NULL);

  gtk_container_add (GTK_CONTAINER(hdy_column), GTK_WIDGET(search_entry));
  gtk_container_add (GTK_CONTAINER(search_bar), GTK_WIDGET(hdy_column));
  gtk_box_pack_start (GTK_BOX (vbox),
                      GTK_WIDGET (search_bar),
                      FALSE, FALSE, 0);

  _chatty_blist->treemodel_contacts = gtk_list_store_new (NUM_COLUMNS,
                                                          G_TYPE_POINTER,
                                                          G_TYPE_OBJECT,
                                                          G_TYPE_STRING,
                                                          G_TYPE_OBJECT);

  filter = gtk_tree_model_filter_new (GTK_TREE_MODEL(_chatty_blist->treemodel_contacts), NULL);

  g_signal_connect (search_entry,
                    "search-changed",
                    G_CALLBACK (cb_search_entry_changed),
                    GTK_TREE_MODEL_FILTER(filter));

  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER(filter),
                                          (GtkTreeModelFilterVisibleFunc)chatty_blist_entry_visible_func,
                                          GTK_ENTRY(search_entry),
                                          NULL);

  treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model (filter));
  gtk_tree_view_set_grid_lines (treeview, GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
  gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW(treeview), TRUE);
  sc = gtk_widget_get_style_context (GTK_WIDGET(treeview));
  gtk_style_context_add_class (sc, "buddy_list");
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

  gtk_box_pack_start (GTK_BOX (vbox),
                      GTK_WIDGET(_chatty_blist->treeview_contacts),
                      TRUE, TRUE, 0);

  gtk_box_pack_start (GTK_BOX (chatty->pane_view_new_chat),
                      GTK_WIDGET (vbox),
                      TRUE, TRUE, 0);

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
  chatty_blist_refresh (list, FALSE);

  purple_blist_set_visible (TRUE);

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

  chatty_blist_chats_hide_node (list, node, TRUE);

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
  guint          color;
  gboolean       blur;

  PurplePresence *presence = purple_buddy_get_presence (buddy);

  ChattyBlistNode *chatty_node = node->ui_data;

  account = purple_buddy_get_account (buddy);
  account_name = purple_account_get_username (account);

  if (!PURPLE_BLIST_NODE_IS_BUDDY (node)) {
    return;
  }

  protocol_id = purple_account_get_protocol_id (account);

  if (g_strcmp0 (protocol_id, "prpl-mm-sms") == 0) {
    color = CHATTY_ICON_COLOR_GREEN;
  } else {
    color = CHATTY_ICON_COLOR_BLUE;
  }

  if (purple_prefs_get_bool (CHATTY_PREFS_ROOT "/blist/greyout_offline_buddies") &&
      !PURPLE_BUDDY_IS_ONLINE(buddy)) {

    blur = TRUE;
  } else {
    blur = FALSE;
  }

  avatar = chatty_icon_get_buddy_icon ((PurpleBlistNode *)buddy,
                                       CHATTY_ICON_SIZE_LARGE,
                                       color,
                                       blur);

  if (purple_prefs_get_bool (CHATTY_PREFS_ROOT "/blist/blur_idle_buddies") &&
      purple_presence_is_idle (presence)) {

    chatty_icon_do_alphashift (avatar, 77);
  }

  alias = purple_buddy_get_alias (buddy);

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
  guint          color;
  gboolean       blur;

  PurplePresence *presence = purple_buddy_get_presence (buddy);

  ChattyBlistNode *chatty_node = node->ui_data;

  account = purple_buddy_get_account (buddy);

  if (!PURPLE_BLIST_NODE_IS_BUDDY (node)) {
    return;
  }

  protocol_id = purple_account_get_protocol_id (account);

  if (g_strcmp0 (protocol_id, "prpl-mm-sms") == 0) {
    color = CHATTY_ICON_COLOR_GREEN;
  } else {
    color = CHATTY_ICON_COLOR_BLUE;
  }

  if (purple_prefs_get_bool (CHATTY_PREFS_ROOT "/blist/greyout_offline_buddies") &&
      !PURPLE_BUDDY_IS_ONLINE(buddy)) {

    blur = TRUE;
  } else {
    blur = FALSE;
  }

  avatar = chatty_icon_get_buddy_icon ((PurpleBlistNode *)buddy,
                                       CHATTY_ICON_SIZE_LARGE,
                                       color,
                                       blur);

  if (purple_prefs_get_bool (CHATTY_PREFS_ROOT "/blist/blur_idle_buddies") &&
      purple_presence_is_idle (presence)) {

    chatty_icon_do_alphashift (avatar, 77);
  }

  b_name = purple_buddy_get_name (buddy);
  alias = purple_buddy_get_alias (buddy);

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

  if (chatty_node->conv.flags & CHATTY_BLIST_NODE_HAS_PENDING_MESSAGE) {
    name = g_strconcat ("<span color='#646464'>",
                        alias,
                        "</span>",
                        "\n",
                        "<small>",
                        last_msg_text,
                        "</small>",
                        NULL);
  } else {
    name = g_strconcat ("<span color='#646464'>",
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

  if (log_data != NULL && chatty_blist_buddy_is_displayable (buddy)) {
    ui = node->ui_data;
    ui->conv.last_message = log_data->msg;
    ui->conv.last_message_name = log_data->name;
    ui->conv.last_msg_timestamp = log_data->time_stamp;

    chatty_blist_chats_update_node (buddy, node);
  } else {
    chatty_blist_chats_hide_node (list, node, TRUE);
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
