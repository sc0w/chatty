/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib.h>
#include <glib/gi18n.h>
#define HANDY_USE_UNSTABLE_API
#include <handy.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <cairo.h>
#include <purple.h>
#include "chatty-message-row.h"
#include "chatty-message-list.h"

#define INDICATOR_WIDTH   60
#define INDICATOR_HEIGHT  40
#define INDICATOR_MARGIN   2
#define MSG_BUBBLE_MAX_RATIO .3

#define LONGPRESS_TIMEOUT 2000


enum {
  PROP_0,
  PROP_TYPE,
  PROP_DISCLAIMER,
  PROP_RULER,
  PROP_INDICATOR,
  PROP_LAST_PROP,
};

static GParamSpec *props[PROP_LAST_PROP];

enum {
  SIGNAL_MESSAGE_ADDED,
  SIGNAL_SCROLL_TOP,
  SIGNAL_LAST_SIGNAL,
};

static guint signals [SIGNAL_LAST_SIGNAL];

typedef struct
{
  GtkBox      *disclaimer;
  GtkWidget   *list;
  GtkWidget   *scroll;
  GtkAdjustment *vadjustment;
  GtkWidget   *button;
  GtkWidget   *indicator_row;
  GtkWidget   *typing_indicator;
  GtkWidget   *label_pressed;
  GtkWidget   *menu_popover;
  gboolean     disclaimer_enable;
  gboolean     ruler_enable;
  gboolean     indicator_enable;
  gboolean     animation_enable;
  guint        message_type;
  guint        width;
  guint        height;
  guint        prev_height;
  guint        longpress_timeout_handle;
  guint32      refresh_timeout_handle;
  gboolean     first_scroll_to_bottom;
} ChattyMsgListPrivate;


G_DEFINE_TYPE_WITH_PRIVATE (ChattyMsgList, chatty_msg_list, GTK_TYPE_BOX)

header_strings_t header_strings[3] = {
  {
    .str_0 = N_("This is a IM conversation."),
    .str_1 = N_("Your messages are not encrypted,"),
    .str_2 = N_("ask your counterpart to use E2EE."),
  },
  {
    .str_0 = N_("This is a IM conversation."),
    .str_1 = N_("Your messages are secured"),
    .str_2 = N_("by end-to-end encryption."),
  },
  {
    .str_0 = N_("This is a SMS conversation."),
    .str_1 = N_("Your messages are not encrypted,"),
    .str_2 = N_("and carrier rates may apply."),
  },
};



static void
cb_scroll_edge_reached (GtkScrolledWindow *scrolled_window,
                        GtkPositionType     pos,
                        gpointer            self)
{

    if (pos == GTK_POS_TOP)    
      g_signal_emit (self,
                   signals[SIGNAL_SCROLL_TOP],
                   0);
}


static void
cb_list_focus (GtkWidget *sender,
               int        direction,
               gpointer   self)
{
  GtkAdjustment *adj;
  gdouble        upper;
  gdouble        size;

  ChattyMsgListPrivate *priv = chatty_msg_list_get_instance_private (self);

  adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->scroll));

  size = gtk_adjustment_get_page_size (adj);
  upper = gtk_adjustment_get_upper (adj);
  gtk_adjustment_set_value (adj, upper - size);
}


static void
chatty_msg_list_add_header (ChattyMsgList *self)
{
  GtkWidget       *label;
  GtkWidget       *label_2;
  GtkWidget       *label_3;
  GtkWidget       *row;
  GtkStyleContext *sc;

  ChattyMsgListPrivate *priv = chatty_msg_list_get_instance_private (self);

  row = gtk_list_box_row_new ();

  g_object_set (G_OBJECT(row),
                "selectable", FALSE,
                "activatable", FALSE,
                NULL);

  gtk_widget_set_size_request (row,
                               1,
                               0); // TODO: set priv->height instead);
                                   // TODO: @LELAND: Talk to Andrea about this header:
                                   // Adding messages backward leves this space at the bottom (320 to 0 by now)


  gtk_container_add (GTK_CONTAINER (priv->list), row);

  if (priv->disclaimer_enable && priv->message_type < CHATTY_MSG_TYPE_LAST) {
    priv->disclaimer = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));

    label = gtk_label_new (_(header_strings[priv->message_type].str_0));
    sc = gtk_widget_get_style_context (GTK_WIDGET(label));
    gtk_style_context_add_class (sc, "label_disclaim");
    gtk_box_pack_start (priv->disclaimer, GTK_WIDGET(label), FALSE, FALSE, 0);

    label_2 = gtk_label_new (_(header_strings[priv->message_type].str_1));
    gtk_box_pack_start (priv->disclaimer, GTK_WIDGET(label_2), FALSE, FALSE, 0);

    label_3 = gtk_label_new (_(header_strings[priv->message_type].str_2));
    gtk_box_pack_start (priv->disclaimer, GTK_WIDGET(label_3), FALSE, FALSE, 0);

    gtk_container_add (GTK_CONTAINER(row), GTK_WIDGET(priv->disclaimer));

    gtk_widget_show_all (GTK_WIDGET(row));
  }
}


static void
list_page_size_changed_cb (ChattyMsgList *self)
{
  ChattyMsgListPrivate *priv = chatty_msg_list_get_instance_private (self);
  gdouble size, upper, value;

  g_assert (CHATTY_IS_MSG_LIST (self));

  size  = gtk_adjustment_get_page_size (priv->vadjustment);
  value = gtk_adjustment_get_value (priv->vadjustment);
  upper = gtk_adjustment_get_upper (priv->vadjustment);

  if (upper - size <= DBL_EPSILON)
    return;

  /* If close to bottom, scroll to bottom */
  if (!priv->first_scroll_to_bottom || upper - value < (size * 1.75))
    gtk_adjustment_set_value (priv->vadjustment, upper);

  priv->first_scroll_to_bottom = TRUE;
}

static void
chatty_msg_list_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  ChattyMsgList *self = CHATTY_MSG_LIST (object);

  ChattyMsgListPrivate *priv = chatty_msg_list_get_instance_private (self);

  switch (property_id) {
    case PROP_TYPE:
      priv->message_type = g_value_get_int (value);
      break;

    case PROP_DISCLAIMER:
      priv->disclaimer_enable = g_value_get_boolean (value);
      chatty_msg_list_add_header (self);
      break;

    case PROP_RULER:
      priv->ruler_enable = g_value_get_boolean (value);
      break;

    case PROP_INDICATOR:
      priv->indicator_enable = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}


static void
chatty_msg_list_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  ChattyMsgList *self = CHATTY_MSG_LIST (object);

  ChattyMsgListPrivate *priv = chatty_msg_list_get_instance_private (self);

  switch (property_id) {
    case PROP_TYPE:
      g_value_set_int (value, priv->message_type);
      break;

    case PROP_DISCLAIMER:
      g_value_set_boolean (value, priv->disclaimer_enable);
      break;

    case PROP_RULER:
      g_value_set_boolean (value, priv->ruler_enable);
      break;

    case PROP_INDICATOR:
      g_value_set_boolean (value, priv->indicator_enable);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
                                         property_id,
                                         pspec);
      break;
  }
}


static void
chatty_msg_list_hide_header (ChattyMsgList *self)
{
  ChattyMsgListPrivate *priv = chatty_msg_list_get_instance_private (self);

  if (GTK_WIDGET(priv->disclaimer) != NULL) {
    gtk_widget_hide (GTK_WIDGET(priv->disclaimer));
  }
}


static void
chatty_draw_typing_indicator (cairo_t *cr)
{
  double dot_pattern [3][3]= {{0.5, 0.9, 0.9},
                              {0.7, 0.5, 0.9},
                              {0.9, 0.7, 0.5}};
  guint  dot_origins [3] = {15, 30, 45};
  double grey_lev,
         x, y,
         width, height,
         rad, deg;

  static guint i;

  deg = M_PI / 180.0;

  rad = INDICATOR_MARGIN * 5;
  x = y = INDICATOR_MARGIN;
  width = INDICATOR_WIDTH - INDICATOR_MARGIN * 2;
  height = INDICATOR_HEIGHT - INDICATOR_MARGIN * 2;

  if (i > 2)
    i = 0;

  cairo_new_sub_path (cr);
  cairo_arc (cr, x + width - rad, y + rad, rad, -90 * deg, 0 * deg);
  cairo_arc (cr, x + width - rad, y + height - rad, rad, 0 * deg, 90 * deg);
  cairo_arc (cr, x + rad, y + height - rad, rad, 90 * deg, 180 * deg);
  cairo_arc (cr, x + rad, y + rad, rad, 180 * deg, 270 * deg);
  cairo_close_path (cr);

  cairo_set_source_rgb (cr, 0.7, 0.7, 0.7);
  cairo_set_line_width (cr, 1.0);
  cairo_stroke (cr);

  for (guint n = 0; n < 3; n++) {
    cairo_arc (cr, dot_origins[n], 20, 5, 0, 2 * M_PI);
    grey_lev = dot_pattern[i][n];
    cairo_set_source_rgb (cr, grey_lev, grey_lev, grey_lev);
    cairo_fill (cr);
  }

  i++;
}


static gboolean
cb_on_draw_event (GtkWidget *widget,
                  cairo_t   *cr,
                  gpointer  self)
{
  ChattyMsgListPrivate *priv = chatty_msg_list_get_instance_private (self);

  if (priv->animation_enable)
    chatty_draw_typing_indicator (cr);

  return TRUE;
}


static gboolean
cb_indicator_refresh (gpointer self)
{
    ChattyMsgListPrivate *priv = chatty_msg_list_get_instance_private (self);

    priv->animation_enable = TRUE;

    gtk_widget_queue_draw (priv->typing_indicator);

    return TRUE;
}


void
chatty_msg_list_show_typing_indicator (ChattyMsgList *self)
{
  GtkWidget   *box;
  GtkRevealer *revealer;

  ChattyMsgListPrivate *priv = chatty_msg_list_get_instance_private (self);

  priv->animation_enable = FALSE;

  priv->indicator_row = gtk_list_box_row_new ();
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (priv->indicator_row), FALSE);
  gtk_list_box_row_set_selectable (GTK_LIST_BOX_ROW (priv->indicator_row), FALSE);

  gtk_container_add (GTK_CONTAINER (priv->list),
                                    GTK_WIDGET (priv->indicator_row));

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_set_border_width (GTK_CONTAINER (box), 4);

  revealer = GTK_REVEALER (gtk_revealer_new ());
  gtk_revealer_set_transition_type (revealer,
                                    GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
  gtk_revealer_set_transition_duration (revealer, 350);

  gtk_container_add (GTK_CONTAINER (revealer),
                     GTK_WIDGET (box));
  gtk_container_add (GTK_CONTAINER (priv->indicator_row),
                     GTK_WIDGET (revealer));

  priv->typing_indicator = gtk_drawing_area_new();

  gtk_widget_set_size_request (priv->typing_indicator,
                               INDICATOR_WIDTH,
                               INDICATOR_HEIGHT);

  g_signal_connect (G_OBJECT (priv->typing_indicator),
                    "draw",
                    G_CALLBACK (cb_on_draw_event),
                    (gpointer) self);

  gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (priv->typing_indicator),
                      FALSE, TRUE, 8);

  chatty_msg_list_hide_header (self);
  gtk_widget_show_all (GTK_WIDGET(priv->indicator_row));
  gtk_revealer_set_reveal_child (revealer, TRUE);

  priv->refresh_timeout_handle = g_timeout_add (300,
                                                (GSourceFunc)(cb_indicator_refresh),
                                                (gpointer) self);
}


void
chatty_msg_list_hide_typing_indicator (ChattyMsgList *self)
{
  ChattyMsgListPrivate *priv = chatty_msg_list_get_instance_private (self);

  if (priv->refresh_timeout_handle) {
      g_source_remove (priv->refresh_timeout_handle);
      priv->refresh_timeout_handle = 0;
  }

  if (priv->typing_indicator != NULL) {
    gtk_widget_hide (GTK_WIDGET(priv->typing_indicator));
    gtk_widget_destroy (GTK_WIDGET(priv->typing_indicator));
    gtk_widget_destroy (GTK_WIDGET(priv->indicator_row));
    priv->typing_indicator = NULL;
  }
}


void
chatty_msg_list_clear (ChattyMsgList *self)
{
  GList *children;
  GList *iter;

  ChattyMsgListPrivate *priv = chatty_msg_list_get_instance_private (self);

  children = gtk_container_get_children (GTK_CONTAINER(priv->list));

  for (iter = children; iter != NULL; iter = g_list_next (iter)) {
    gtk_container_remove (GTK_CONTAINER(priv->list), GTK_WIDGET(iter->data));
  }

  g_list_free (children);
  g_list_free (iter);
}

 
/* This removes all markup exept for links */
static gchar *
chatty_msg_list_escape_message (const gchar *message)
{
  g_autofree gchar  *nl_2_br;
  g_autofree gchar  *striped;
  g_autofree gchar  *escaped;
  g_autofree gchar  *linkified;
  gchar *result;

  nl_2_br = purple_strdup_withhtml (message);
  striped = purple_markup_strip_html (nl_2_br);
  escaped = purple_markup_escape_text (striped, -1);
  linkified = purple_markup_linkify (escaped);
  // convert all tags to lowercase for GtkLabel markup parser
  purple_markup_html_to_xhtml (linkified, &result, NULL);

  return result;
}

GtkWidget *
chatty_msg_list_add_message_at (ChattyMsgList *self,
                                guint          message_dir,
                                const gchar   *html_message,
                                const gchar   *footer,
                                GdkPixbuf     *avatar,
                                guint          position)
{
  GtkWidget *row;
  g_autofree char *message = NULL;

  ChattyMsgListPrivate *priv = chatty_msg_list_get_instance_private (self);

  /* Don’t set avatar for IM chats */
  if (priv->message_type == CHATTY_MSG_TYPE_IM ||
      priv->message_type == CHATTY_MSG_TYPE_SMS)
    avatar = NULL;

  message = chatty_msg_list_escape_message (html_message);
  row = chatty_message_row_new ();

  if (position == ADD_MESSAGE_ON_BOTTOM){
    gtk_container_add (GTK_CONTAINER (priv->list), GTK_WIDGET (row));
  } else {
    gtk_list_box_prepend (GTK_LIST_BOX (priv->list), GTK_WIDGET (row));
  }

  chatty_message_row_set_item (CHATTY_MESSAGE_ROW (row), message_dir,
                               priv->message_type, message,
                               footer, avatar);

  if (message_dir == MSG_IS_OUTGOING) {
    g_signal_emit (self,
                   signals[SIGNAL_MESSAGE_ADDED],
                   0,
                   G_OBJECT (row));
  }

  chatty_msg_list_hide_header (self);

  return row;
}


GtkWidget *
chatty_msg_list_add_message (ChattyMsgList *self,
                             guint          message_dir,
                             const gchar   *message,
                             const gchar   *footer,
                             GdkPixbuf     *avatar)
{
  return chatty_msg_list_add_message_at (self, message_dir, message, footer, avatar, ADD_MESSAGE_ON_BOTTOM);
}


static void
chatty_msg_list_constructed (GObject *object)
{
  GtkStyleContext      *sc;
  ChattyMsgList        *self = CHATTY_MSG_LIST (object);
  ChattyMsgListPrivate *priv = chatty_msg_list_get_instance_private (self);

  gtk_widget_set_valign (GTK_WIDGET(priv->list), GTK_ALIGN_END);
  sc = gtk_widget_get_style_context (GTK_WIDGET(priv->list));

  gtk_style_context_add_class (sc, "message_list");
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (priv->list), GTK_SELECTION_NONE);
  g_signal_connect_object (GTK_WIDGET (priv->list),
                           "focus",
                           G_CALLBACK (cb_list_focus),
                           (gpointer) self, 0);

  g_signal_connect (GTK_SCROLLED_WINDOW (priv->scroll),
                    "edge-overshot",
                    G_CALLBACK(cb_scroll_edge_reached),
                    self);
}


static void
chatty_msg_list_class_init (ChattyMsgListClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = chatty_msg_list_constructed;

  object_class->set_property = chatty_msg_list_set_property;
  object_class->get_property = chatty_msg_list_get_property;

  props[PROP_TYPE] =
    g_param_spec_int ("message_type",
                      "Message Type",
                      "Select the message type",
                      CHATTY_MSG_TYPE_IM, CHATTY_MSG_TYPE_LAST, CHATTY_MSG_TYPE_IM,
                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_DISCLAIMER] =
    g_param_spec_boolean ("disclaimer",
                          "Messagemode Disclaimer",
                          "Enables a disclaimer with privacy advice",
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_RULER] =
    g_param_spec_boolean ("ruler",
                          "Timestamp Ruler",
                          "Enables a ruler that shows a timestamp",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_INDICATOR] =
    g_param_spec_boolean ("indicator",
                          "Typing Indicator",
                          "Enables the typing indicator",
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);

  signals[SIGNAL_MESSAGE_ADDED] =
    g_signal_new ("message-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_OBJECT);
  
  signals[SIGNAL_SCROLL_TOP] =
    g_signal_new ("scroll-top",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  gtk_widget_class_set_template_from_resource (widget_class,
    "/sm/puri/chatty/ui/chatty-message-list.ui");

  gtk_widget_class_bind_template_child_private (widget_class,
                                                ChattyMsgList,
                                                scroll);
  gtk_widget_class_bind_template_child_private (widget_class,
                                                ChattyMsgList,
                                                list);
  gtk_widget_class_bind_template_child_private (widget_class,
                                                ChattyMsgList,
                                                vadjustment);
  gtk_widget_class_bind_template_callback (widget_class, list_page_size_changed_cb);
}


void
chatty_msg_list_set_msg_type (ChattyMsgList *self,
                              guint         message_type)
{
  g_return_if_fail (CHATTY_IS_MSG_LIST (self));

  g_object_set (G_OBJECT (self), "message_type", message_type, NULL);
}


guint
chatty_msg_list_get_msg_type (ChattyMsgList *self)
{
  ChattyMsgListPrivate *priv = chatty_msg_list_get_instance_private (self);

  g_return_val_if_fail (CHATTY_IS_MSG_LIST (self), CHATTY_MSG_TYPE_UNKNOWN);

  return priv->message_type;
}


static void
chatty_msg_list_init (ChattyMsgList *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}


GtkWidget *
chatty_msg_list_new (guint message_type,
                     gboolean disclaimer)
{
  return g_object_new (CHATTY_TYPE_MSG_LIST,
                       "message_type",
                       message_type,
                       "disclaimer",
                       disclaimer,
                       NULL);
}
