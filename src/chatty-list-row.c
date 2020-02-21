/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-list-row.c
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "users/chatty-pp-buddy.h"
#include "users/chatty-contact.h"
#include "chatty-chat.h"
#include "chatty-avatar.h"
#include "chatty-list-row.h"

struct _ChattyListRow
{
  GtkListBoxRow  parent_instance;

  GtkWidget     *avatar;
  GtkWidget     *title;
  GtkWidget     *subtitle;
  GtkWidget     *last_modified;
  GtkWidget     *unread_message_count;

  ChattyItem    *item;
};

G_DEFINE_TYPE (ChattyListRow, chatty_list_row, GTK_TYPE_LIST_BOX_ROW)


static void
chatty_list_row_update (ChattyListRow *self)
{
  PurpleAccount *pp_account;
  const char *subtitle = NULL;

  g_assert (CHATTY_IS_LIST_ROW (self));
  g_assert (CHATTY_IS_ITEM (self->item));

  if (CHATTY_IS_PP_BUDDY (self->item)) {
    pp_account = chatty_pp_buddy_get_account (CHATTY_PP_BUDDY (self->item));
    subtitle = purple_account_get_username (pp_account);
  }

  if (subtitle)
    gtk_label_set_label (GTK_LABEL (self->subtitle), subtitle);
}

static void
chatty_list_row_finalize (GObject *object)
{
  ChattyListRow *self = (ChattyListRow *)object;

  g_clear_object (&self->item);

  G_OBJECT_CLASS (chatty_list_row_parent_class)->finalize (object);
}

static void
chatty_list_row_class_init (ChattyListRowClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = chatty_list_row_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/chatty/"
                                               "ui/chatty-list-row.ui");
  gtk_widget_class_bind_template_child (widget_class, ChattyListRow, avatar);
  gtk_widget_class_bind_template_child (widget_class, ChattyListRow, title);
  gtk_widget_class_bind_template_child (widget_class, ChattyListRow, subtitle);
  gtk_widget_class_bind_template_child (widget_class, ChattyListRow, last_modified);
  gtk_widget_class_bind_template_child (widget_class, ChattyListRow, unread_message_count);
}

static void
chatty_list_row_init (ChattyListRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
chatty_list_row_new (ChattyItem *item)
{
  ChattyListRow *self;

  g_return_val_if_fail (CHATTY_IS_CONTACT (item) ||
                        CHATTY_IS_PP_BUDDY (item) ||
                        CHATTY_IS_CHAT (item), NULL);

  self = g_object_new (CHATTY_TYPE_LIST_ROW, NULL);
  self->item = g_object_ref (item);
  chatty_avatar_set_item (CHATTY_AVATAR (self->avatar), item);
  g_object_bind_property (item, "name",
                          self->title, "label",
                          G_BINDING_SYNC_CREATE);

  chatty_list_row_update (self);

  return GTK_WIDGET (self);
}


ChattyItem *
chatty_list_row_get_item (ChattyListRow *self)
{
  g_return_val_if_fail (CHATTY_IS_LIST_ROW (self), NULL);

  return self->item;
}
