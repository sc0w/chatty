/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-message-row.c
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *   Andrea Schäfer <mosibasu@me.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib/gi18n.h>

#include "chatty-message-row.h"


struct _ChattyMessageRow
{
  GtkListBoxRow  parent_instance;

  GtkWidget  *revealer;
  GtkWidget  *content_grid;
  GtkWidget  *avatar_image;
  GtkWidget  *message_event_box;
  GtkWidget  *message_label;
  GtkWidget  *footer_label;

  GtkWidget  *popover;
  GtkGesture *multipress_gesture;
  GtkGesture *longpress_gesture;
};

G_DEFINE_TYPE (ChattyMessageRow, chatty_message_row, GTK_TYPE_LIST_BOX_ROW)


static void
copy_clicked_cb (ChattyMessageRow *self)
{
  GtkClipboard *clipboard;
  const char *text;

  g_assert (CHATTY_IS_MESSAGE_ROW (self));

  clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
  text = gtk_label_get_text (GTK_LABEL (self->message_label));

  if (text != NULL)
    gtk_clipboard_set_text (clipboard, text, -1);
}

static void
message_row_show_popover (ChattyMessageRow *self)
{
  g_assert (CHATTY_IS_MESSAGE_ROW (self));

  if (!self->popover) {
    GtkWidget *item;

    self->popover = gtk_popover_new (self->message_event_box);
    item = g_object_new (GTK_TYPE_MODEL_BUTTON,
                         "margin", 12,
                         "text", _("Copy"),
                         NULL);
    gtk_widget_show (item);
    g_signal_connect_swapped (item, "clicked",
                              G_CALLBACK (copy_clicked_cb), self);
    gtk_container_add (GTK_CONTAINER (self->popover), item);
  }

  gtk_popover_popup (GTK_POPOVER (self->popover));
}


static void
message_row_hold_cb (ChattyMessageRow *self)
{
  g_assert (CHATTY_IS_MESSAGE_ROW (self));

  message_row_show_popover (self);
  gtk_gesture_set_state (self->longpress_gesture, GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
chatty_message_row_finalize (GObject *object)
{
  ChattyMessageRow *self = (ChattyMessageRow *)object;

  g_clear_object (&self->multipress_gesture);
  g_clear_object (&self->longpress_gesture);

  G_OBJECT_CLASS (chatty_message_row_parent_class)->finalize (object);
}

static void
chatty_message_row_class_init (ChattyMessageRowClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = chatty_message_row_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/chatty/"
                                               "ui/chatty-message-row.ui");
  gtk_widget_class_bind_template_child (widget_class, ChattyMessageRow, revealer);
  gtk_widget_class_bind_template_child (widget_class, ChattyMessageRow, content_grid);
  gtk_widget_class_bind_template_child (widget_class, ChattyMessageRow, avatar_image);
  gtk_widget_class_bind_template_child (widget_class, ChattyMessageRow, message_event_box);
  gtk_widget_class_bind_template_child (widget_class, ChattyMessageRow, message_label);
  gtk_widget_class_bind_template_child (widget_class, ChattyMessageRow, footer_label);
}

static void
chatty_message_row_init (ChattyMessageRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->multipress_gesture = gtk_gesture_multi_press_new (self->message_event_box);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (self->multipress_gesture), GDK_BUTTON_SECONDARY);
  g_signal_connect_swapped (self->multipress_gesture, "pressed",
                            G_CALLBACK (message_row_show_popover), self);

  self->longpress_gesture = gtk_gesture_long_press_new (self->message_event_box);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (self->longpress_gesture), TRUE);
  g_signal_connect_swapped (self->longpress_gesture, "pressed",
                            G_CALLBACK (message_row_hold_cb), self);
}

GtkWidget *
chatty_message_row_new (void)
{
  return g_object_new (CHATTY_TYPE_MESSAGE_ROW, NULL);
}


void
chatty_message_row_set_footer (ChattyMessageRow *self,
                               GtkWidget        *footer)
{
  g_return_if_fail (CHATTY_IS_MESSAGE_ROW (self));

  if (!footer || footer == self->footer_label)
    return;

  gtk_container_remove (GTK_CONTAINER (self->content_grid), self->footer_label);
  gtk_grid_attach (GTK_GRID (self->content_grid), footer, 1, 1, 1, 1);
  gtk_widget_show (footer);
  self->footer_label = footer;
}

void
chatty_message_row_set_item (ChattyMessageRow *self,
                             guint             message_dir,
                             e_msg_type        message_type,
                             const char       *message,
                             const char       *footer,
                             GdkPixbuf        *avatar)
{
  GtkStyleContext *sc;

  g_return_if_fail (CHATTY_IS_MESSAGE_ROW (self));

  gtk_label_set_markup (GTK_LABEL (self->message_label), message);
  gtk_widget_set_visible (self->footer_label, footer && *footer);
  gtk_widget_set_visible (self->avatar_image, avatar != NULL);

  if (footer)
    gtk_label_set_text (GTK_LABEL (self->footer_label), footer);

  sc = gtk_widget_get_style_context (self->message_label);

  if (message_dir == MSG_IS_INCOMING) {
    gtk_style_context_add_class (sc, "bubble_white");
    gtk_image_set_from_pixbuf (GTK_IMAGE (self->avatar_image), avatar);
    gtk_widget_set_halign (self->content_grid, GTK_ALIGN_START);
  } else if (message_dir == MSG_IS_OUTGOING && message_type == CHATTY_MSG_TYPE_SMS) {
    gtk_style_context_add_class (sc, "bubble_green");
  } else if (message_dir == MSG_IS_OUTGOING) {
    gtk_style_context_add_class (sc, "bubble_blue");
  } else { /* System message */
    gtk_style_context_add_class (sc, "bubble_purple");
    gtk_widget_set_hexpand (self->message_label, TRUE);
    gtk_widget_hide (self->avatar_image);
  }

  if (message_dir == MSG_IS_OUTGOING) {
    gtk_label_set_xalign (GTK_LABEL (self->footer_label), 1);
    gtk_widget_set_halign (self->content_grid, GTK_ALIGN_END);
  } else {
    gtk_label_set_xalign (GTK_LABEL (self->footer_label), 0);
  }

  gtk_widget_show_all (GTK_WIDGET (self));
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), TRUE);
}
