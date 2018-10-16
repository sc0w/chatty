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


static gboolean _editing_blist = FALSE;
static ChattyBuddyList *_chatty_blist = NULL;

static void chatty_blist_new_node (PurpleBlistNode *node);
static void chatty_blist_add_buddy_clear_entries (void);

static void chatty_blist_update (PurpleBuddyList *list,
                                 PurpleBlistNode *node);

static void chatty_blist_hide_node (PurpleBuddyList *list,
                                    PurpleBlistNode *node,
                                    gboolean        update);

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
  PurpleBlistNode *node;
  GtkTreeIter     iter;
  GdkPixbuf       *avatar;

  chatty_data_t *chatty = chatty_get_data ();

  gtk_tree_model_get_iter (GTK_TREE_MODEL(_chatty_blist->treemodel),
                           &iter,
                           path);

  gtk_tree_model_get (GTK_TREE_MODEL(_chatty_blist->treemodel),
                      &iter,
                      COLUMN_NODE,
                      &node,
                      -1);

  if (PURPLE_BLIST_NODE_IS_CONTACT(node) || PURPLE_BLIST_NODE_IS_BUDDY(node)) {
    PurpleBuddy *buddy;

    if (PURPLE_BLIST_NODE_IS_CONTACT(node)) {
      buddy = purple_contact_get_priority_buddy ((PurpleContact*)node);
    } else {
      buddy = (PurpleBuddy*)node;
    }

    chatty_conv_im_with_buddy (purple_buddy_get_account (buddy),
                               purple_buddy_get_name (buddy));

    chatty_window_change_view (CHATTY_VIEW_MESSAGE_LIST);
    chatty_window_set_header_title (purple_buddy_get_name (buddy));

    avatar = chatty_icon_get_buddy_icon ((PurpleBlistNode *)buddy,
                                         CHATTY_PRPL_ICON_MEDIUM,
                                         TRUE);

    if (avatar != NULL) {
      gtk_image_set_from_pixbuf (GTK_IMAGE(chatty->header_icon), avatar);
      g_object_unref (avatar);
    } else {
      gtk_image_clear (GTK_IMAGE(chatty->header_icon));
    }
  }
}


static void
cb_button_new_conversation_clicked (GtkButton *sender,
                                    gpointer   account)
{
  chatty_blist_add_buddy (account);
  chatty_window_change_view (CHATTY_VIEW_CONVERSATIONS_LIST);
  chatty_blist_add_buddy_clear_entries();
}


static void
cb_buddy_name_insert_text (GtkEntry *entry,
                           const    gchar *text,
                           gint     length,
                           gint     *position,
                           gpointer data)
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

  if (ui_data == NULL || ui_data->row == NULL) {
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
                         PurpleConvUpdateType type,
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
}

static void
cb_written_msg_update_ui (PurpleAccount       *account,
                          const char          *who,
                          const char          *message,
                          PurpleConversation  *conv,
                          PurpleMessageFlags   flag,
                          PurpleBlistNode     *node)
{
  GDateTime              *local_time;

  ChattyBlistNode *ui = node->ui_data;

  if (ui->conv.conv != conv || !(flag & (PURPLE_MESSAGE_RECV))) {
    return;
  }

  local_time = g_date_time_new_now_local ();

  ui->conv.last_msg_timestamp = g_date_time_format (local_time, "%R");
  ui->conv.flags |= CHATTY_BLIST_NODE_HAS_PENDING_MESSAGE;
  ui->conv.pending_messages ++;

  chatty_blist_update (purple_get_blist(), node);
}


static void
cb_displayed_msg_update_ui (ChattyConversation *gtkconv,
                            PurpleBlistNode    *node)
{
  ChattyBlistNode *ui = node->ui_data;

  if (ui->conv.conv != gtkconv->active_conv)
    return;

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

      if (!ui)
        continue;

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

  if (!gtk_tree_model_iter_children (GTK_TREE_MODEL(_chatty_blist->treemodel),
                                     &iter,
                                     NULL)) {
    return TRUE;
  }

  do {
    gtk_tree_model_get (GTK_TREE_MODEL(_chatty_blist->treemodel),
                        &iter,
                        COLUMN_NODE,
                        &node,
                        -1);

    chatty_node = node->ui_data;

    path =
      gtk_tree_model_get_path (GTK_TREE_MODEL(_chatty_blist->treemodel), &iter);

    gtk_tree_view_get_cell_area (_chatty_blist->treeview,
                                 path,
                                 NULL,
                                 &rect);

    if (chatty_node->conv.pending_messages) {
      chatty_blist_draw_notification_badge (cr,
                                            rect.y,
                                            rect.height,
                                            chatty_node->conv.pending_messages);
    }

  } while (gtk_tree_model_iter_next (GTK_TREE_MODEL(_chatty_blist->treemodel),
                                     &iter));

  return TRUE;
}


// *** end callbacks


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
    gtk_widget_get_allocation (GTK_WIDGET(_chatty_blist->treeview), alloc);

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


static gboolean
chatty_blist_buddy_is_displayable (PurpleBuddy *buddy)
{
  struct _chatty_blist_node *chatty_node;

  if(!buddy) {
    return FALSE;
  }

  chatty_node = ((PurpleBlistNode*)buddy)->ui_data;

  return (purple_account_is_connected (buddy->account) &&
          (purple_presence_is_online (buddy->presence) ||
           (chatty_node && chatty_node->recent_signonoff) ||
           purple_prefs_get_bool (CHATTY_PREFS_ROOT "/blist/show_offline_buddies")));

}


/**
 * chatty_blist_add_buddy:
 *
 * Add a new buddy to the blist
 *
 */
void
chatty_blist_add_buddy (PurpleAccount *account)
{
  const gchar        *who, *whoalias, *invite;
  PurpleBuddy        *b;
  PurpleConversation *c;
  PurpleBuddyIcon    *icon;

  chatty_data_t *chatty = chatty_get_data ();

  who = gtk_entry_get_text (GTK_ENTRY(chatty->entry_buddy_name));
  whoalias = gtk_entry_get_text (GTK_ENTRY(chatty->entry_buddy_nick));

  if (*whoalias == '\0') {
    whoalias = NULL;
  }

  invite = gtk_entry_get_text (GTK_ENTRY(chatty->entry_invite_msg));

  if (*invite == '\0') {
    invite = NULL;
  }

  b = purple_buddy_new (account, who, whoalias);
  purple_blist_add_buddy (b, NULL, NULL, NULL);

  purple_account_add_buddy_with_invite (account, b, invite);

  c = purple_find_conversation_with_account (PURPLE_CONV_TYPE_IM,
                                             who,
                                             account);

  if (c != NULL) {
    icon = purple_conv_im_get_icon (PURPLE_CONV_IM(c));

    if (icon != NULL) {
      purple_buddy_icon_update (icon);
    }
  }
}


static void
chatty_blist_add_buddy_clear_entries (void) {
  chatty_data_t *chatty = chatty_get_data ();

  gtk_entry_set_text (GTK_ENTRY(chatty->entry_buddy_name), "");
  gtk_entry_set_text (GTK_ENTRY(chatty->entry_buddy_nick), "");
  gtk_entry_set_text (GTK_ENTRY(chatty->entry_invite_msg), "");
}


void
chatty_blist_create_add_buddy_view (PurpleAccount *account,
                                    gboolean       invite_enabled)
{
  GtkWidget   *grid;
  GtkWidget   *button_avatar;

  chatty_data_t *chatty = chatty_get_data ();

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 20);

  button_avatar = chatty_icon_get_avatar_button (80);

  gtk_grid_attach (GTK_GRID (grid), button_avatar, 0, 1, 1, 1);

  chatty->entry_buddy_name = GTK_ENTRY (gtk_entry_new ());
  gtk_entry_set_placeholder_text (GTK_ENTRY (chatty->entry_buddy_name),
                                  "id@any-server.com");

  g_signal_connect (G_OBJECT(chatty->entry_buddy_name),
                    "insert_text",
                    G_CALLBACK(cb_buddy_name_insert_text),
                    NULL);

  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (chatty->entry_buddy_name), 0, 2, 1, 1);

  chatty->entry_buddy_nick = GTK_ENTRY (gtk_entry_new ());
  gtk_entry_set_placeholder_text (GTK_ENTRY (chatty->entry_buddy_nick),
                                  _("Nickname (optional)"));
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (chatty->entry_buddy_nick), 0, 3, 1, 1);

  chatty->entry_invite_msg = GTK_ENTRY (gtk_entry_new ());
  gtk_widget_set_sensitive (GTK_WIDGET (chatty->entry_invite_msg), invite_enabled);
  gtk_entry_set_placeholder_text (GTK_ENTRY (chatty->entry_invite_msg),
                                  _("Invite message"));
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (chatty->entry_invite_msg), 0, 4, 1, 1);

  chatty->button_add_buddy = gtk_button_new_with_label ("Add");
  gtk_grid_attach (GTK_GRID (grid), chatty->button_add_buddy, 0, 5, 1, 1);
  gtk_widget_set_sensitive (GTK_WIDGET (chatty->button_add_buddy), FALSE);
  g_signal_connect (chatty->button_add_buddy,
                    "clicked",
                    G_CALLBACK (cb_button_new_conversation_clicked),
                    (gpointer) account);

  gtk_widget_set_halign (GTK_WIDGET (grid), GTK_ALIGN_CENTER);
  gtk_widget_set_valign (GTK_WIDGET (grid), GTK_ALIGN_CENTER);

  gtk_widget_show_all (grid);

  gtk_box_pack_start (GTK_BOX (chatty->pane_view_new_conversation),
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

  if(!_chatty_blist || !_chatty_blist->treeview) {
    return;
  }

  node = list->root;

  while (node)
  {
    if (remove && !PURPLE_BLIST_NODE_IS_GROUP(node)) {
      chatty_blist_hide_node (list, node, FALSE);
    }

    if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
      chatty_blist_update_buddy (list, node);
    } else if (PURPLE_BLIST_NODE_IS_GROUP (node)) {
      chatty_blist_update (list, node);
    }

    node = purple_blist_node_next (node, FALSE);
  }
}


/**
 * chatty_blist_hide_node:
 * @list:   a PurpleBuddyList
 * @node:   a PurpleBlistNode
 * @update: a gboolean
 *
 * Hides a node in the blist
 *
 */
static void
chatty_blist_hide_node (PurpleBuddyList *list,
                        PurpleBlistNode *node,
                        gboolean        update)
{
  GtkTreeIter iter;
  GtkTreePath *path;

  ChattyBlistNode *chatty_node = node->ui_data;

  if (!chatty_node || !chatty_node->row || !_chatty_blist) {
    return;
  }

  if (_chatty_blist->selected_node == node) {
    _chatty_blist->selected_node = NULL;
  }

  path = gtk_tree_row_reference_get_path (chatty_node->row);

  if (path == NULL) {
    return;
  }

  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL(_chatty_blist->treemodel), &iter, path)) {
    gtk_tree_path_free (path);
    return;
  }

  gtk_list_store_remove (_chatty_blist->treemodel, &iter);

  gtk_tree_path_free (path);

  if(update && PURPLE_BLIST_NODE_IS_BUDDY(node)) {
    chatty_blist_update (list, node);
  }

  gtk_tree_row_reference_free (chatty_node->row);

  chatty_node->row = NULL;
}


/**
 * chatty_blist_add_columns:
 * @treeview:  a GtkTreeView
 *
 * Setup the columns for the the blist treeview.
 *
 */
static void
chatty_blist_add_columns (GtkTreeView *treeview)
{
  GtkCellRenderer   *renderer;
  GtkTreeViewColumn *column;

  renderer = gtk_cell_renderer_pixbuf_new ();
  column = gtk_tree_view_column_new_with_attributes ("Avatar",
                                                     renderer,
                                                     "pixbuf",
                                                      COLUMN_AVATAR,
                                                      NULL);

  gtk_tree_view_column_set_sort_column_id (column, COLUMN_NAME);
  gtk_cell_renderer_set_padding (renderer, 12, 12);
  gtk_tree_view_append_column (treeview, column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Name",
                                                     renderer,
                                                     "text",
                                                      COLUMN_NAME,
                                                      NULL);

  gtk_tree_view_column_set_sort_column_id (column, COLUMN_NAME);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "markup", COLUMN_NAME,
                                       NULL);

  gtk_cell_renderer_set_alignment (renderer, 0.0, 0.2);
  gtk_tree_view_append_column (treeview, column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Time",
                                                     renderer,
                                                     "text",
                                                      COLUMN_TIME,
                                                      NULL);

  gtk_tree_view_column_set_sort_column_id (column, COLUMN_NAME);
  gtk_tree_view_column_set_attributes (column,
                                       renderer,
                                       "markup", COLUMN_TIME,
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
 * chatty_blist_show:
 * @list:  a PurpleBuddyList
 *
 * Sets up the view with the blist treeview
 * Function is called via PurpleBlistUiOps.
 *
 */
static void
chatty_blist_show (PurpleBuddyList *list)
{
  void              *handle;
  GtkTreeView       *treeview;
  GtkStyleContext   *sc;
  chatty_data_t *chatty = chatty_get_data ();

  _chatty_blist = CHATTY_BLIST(list);

  _chatty_blist->empty_avatar = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 32, 32);
  gdk_pixbuf_fill (_chatty_blist->empty_avatar, 0x00000000);

  _chatty_blist->scroll = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new ( NULL, NULL));
  gtk_widget_show (GTK_WIDGET(_chatty_blist->scroll));

  _chatty_blist->treemodel = gtk_list_store_new (NUM_COLUMNS,
                                                 G_TYPE_POINTER,
                                                 G_TYPE_OBJECT,
                                                 G_TYPE_STRING,
                                                 G_TYPE_STRING);

  treeview = GTK_TREE_VIEW (gtk_tree_view_new_with_model (GTK_TREE_MODEL(_chatty_blist->treemodel)));
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

  _chatty_blist->treeview = treeview;
  gtk_widget_show (GTK_WIDGET(_chatty_blist->treeview));

  gtk_widget_set_name (GTK_WIDGET(_chatty_blist->treeview),
                       "chatty_blist_treeview");

  gtk_container_add (GTK_CONTAINER(_chatty_blist->scroll),
                     GTK_WIDGET(_chatty_blist->treeview));

  chatty_blist_add_columns (GTK_TREE_VIEW (treeview));

  gtk_tree_view_columns_autosize (GTK_TREE_VIEW (treeview));

  gtk_box_pack_start (GTK_BOX (chatty->pane_view_buddy_list),
                      GTK_WIDGET (_chatty_blist->scroll),
                      TRUE, TRUE, 0);


  chatty_blist_refresh (list, FALSE);
  purple_blist_set_visible (TRUE);

  _chatty_blist->refresh_timer =
    purple_timeout_add_seconds(30,
                               (GSourceFunc)cb_chatty_blist_refresh_timer,
                               list);

  handle = chatty_blist_get_handle();

  handle = purple_connections_get_handle();
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

  chatty_blist_hide_node (list, node, TRUE);

  if(chatty_node) {
    if(chatty_node->recent_signonoff_timer > 0) {
      purple_timeout_remove(chatty_node->recent_signonoff_timer);
    }

    purple_signals_disconnect_by_handle (node->ui_data);

    g_free (node->ui_data);
    node->ui_data = NULL;
  }
}


/**
 * chatty_blist_update_node:
 * @buddy: a PurpleBuddy
 * @node:  a PurpleBlistNode
 *
 * Inserts or updates a buddy node in the blist.
 *
 */
static void
chatty_blist_update_node (PurpleBuddy     *buddy,
                          PurpleBlistNode *node)
{
  GtkTreeIter    iter;
  GdkPixbuf     *avatar;
  GtkTreePath   *path;
  const gchar   *name = NULL;
  const gchar   *account_name;
  gchar         *last_msg_str;
  PurpleAccount *account;

  PurplePresence *presence = purple_buddy_get_presence (buddy);

  ChattyBlistNode *chatty_node = node->ui_data;

  if (_editing_blist || (!PURPLE_BLIST_NODE_IS_BUDDY (node))) {
    return;
  }

  avatar = chatty_icon_get_buddy_icon ((PurpleBlistNode *)buddy,
                                       CHATTY_PRPL_ICON_LARGE,
                                       TRUE);

  if (!avatar) {
    g_object_ref (G_OBJECT(_chatty_blist->empty_avatar));
    avatar = _chatty_blist->empty_avatar;
  } else if ((!PURPLE_BUDDY_IS_ONLINE(buddy) ||
    purple_presence_is_idle (presence))) {
    chatty_icon_do_alphashift (avatar, 77);
  }

  name = purple_buddy_get_alias (buddy);

  last_msg_str = chatty_node->conv.last_msg_timestamp;

  account = purple_buddy_get_account (buddy);
  account_name = purple_account_get_username (account);

  if (chatty_node->conv.flags & CHATTY_BLIST_NODE_HAS_PENDING_MESSAGE) {
    name = g_strconcat ("<b><span color='#646464'>",
                        name,
                        "</span></b>",
                        " ",
                        "<small><span color='darkgrey'>",
                        "(",
                        account_name,
                        ")",
                        "</span></small>",
                        NULL);
  } else {
    name = g_strconcat ("<span color='#646464'>",
                        name,
                        "</span>",
                        " ",
                        "<small><span color='darkgrey'>",
                        "(",
                        account_name,
                        ")",
                        "</span></small>",
                        NULL);
  }

  if (last_msg_str != NULL) {
    last_msg_str = g_strconcat ("<span color='darkgrey'>",
                                last_msg_str, "</span>",
                                NULL);
  }

  if (!chatty_node->row) {
    gtk_list_store_append (_chatty_blist->treemodel, &iter);

    path =
      gtk_tree_model_get_path (GTK_TREE_MODEL(_chatty_blist->treemodel), &iter);
    chatty_node->row =
      gtk_tree_row_reference_new (GTK_TREE_MODEL(_chatty_blist->treemodel), path);
  } else {
    path = gtk_tree_row_reference_get_path (chatty_node->row);
    if (path != NULL) {
      gtk_tree_model_get_iter (GTK_TREE_MODEL(_chatty_blist->treemodel), &iter, path);
    }
  }

  if (path != NULL) {
    gtk_list_store_set (_chatty_blist->treemodel, &iter,
                        COLUMN_NODE, node,
                        COLUMN_AVATAR, avatar,
                        COLUMN_NAME, name,
                        COLUMN_TIME, last_msg_str,
                        -1);
  }

  if (avatar) {
    g_object_unref (avatar);
  }

  gtk_tree_path_free (path);
  g_free (last_msg_str);
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
  PurpleBuddy *buddy;

  g_return_if_fail (PURPLE_BLIST_NODE_IS_BUDDY(node));

  buddy = (PurpleBuddy*)node;

  if (chatty_blist_buddy_is_displayable (buddy)) {
    chatty_blist_update_node (buddy, node);
  } else {
    chatty_blist_hide_node (list, node, TRUE);
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
  if (list) {
    _chatty_blist = CHATTY_BLIST(list);
  }

  if (!_chatty_blist || !_chatty_blist->treeview || !node) {
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
 * Function is called via PurpleBlistUiOps.
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
  _chatty_blist->treeview = NULL;
  g_object_unref (G_OBJECT(_chatty_blist->treemodel));
  _chatty_blist->treemodel = NULL;
  g_object_unref (G_OBJECT(_chatty_blist->empty_avatar));

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
 * Function is called via PurpleBlistUiOps.
 *
 */
static void
chatty_blist_request_add_buddy (PurpleAccount *account,
                                const char    *username,
                                const char    *group,
                                const char    *alias)
{
  // TODO remove this callback when the similiar request triggering
  //      via PurpleAccountUiOps is working

  g_debug ("chatty_blist_request_add_buddy: %s  %s  %s",
           username, group, alias);
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
