/*
 * Copyright (C) 2019 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * 
 * Author: Julian Sparber <julian@sparber.net>
 */

#include "chatty-contact-row.h"

enum {
  PROP_0,
  PROP_AVATR,
  PROP_NAME,
  PROP_DESCRIPTION,
  PROP_TIMESTAMP,
  PROP_MESSAGE_COUNT,
  PROP_DATA,
  PROP_LAST_PROP,
};

static GParamSpec *props[PROP_LAST_PROP];

typedef struct
{ 
  GtkWidget *avatar;
  GtkWidget *name;
  GtkWidget *description;
  GtkWidget *timestamp;
  GtkWidget *message_count;

  gpointer data;
} ChattyContactRowPrivate;

struct _ChattyContactRow
{
  GtkListBoxRow parent_instance;
};
G_DEFINE_TYPE_WITH_PRIVATE (ChattyContactRow, chatty_contact_row, GTK_TYPE_LIST_BOX_ROW)


static void
chatty_contact_row_get_property (GObject      *object,
                                 guint         property_id,
                                 GValue *value,
                                 GParamSpec   *pspec)
{
  ChattyContactRow *self = CHATTY_CONTACT_ROW (object);
  ChattyContactRowPrivate *priv = chatty_contact_row_get_instance_private (self);

  switch (property_id) {
    case PROP_AVATR:
      g_value_set_object (value, gtk_image_get_pixbuf (GTK_IMAGE (priv->avatar)));
      break;

    case PROP_NAME:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (priv->name)));
      break;

    case PROP_DESCRIPTION:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (priv->description)));
      break;

    case PROP_TIMESTAMP:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (priv->timestamp)));
      break;

    case PROP_MESSAGE_COUNT:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (priv->message_count)));
      break;

    case PROP_DATA:
      g_value_set_pointer (value, priv->data);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}


static void
chatty_contact_row_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ChattyContactRow *self = CHATTY_CONTACT_ROW (object);

  ChattyContactRowPrivate *priv = chatty_contact_row_get_instance_private (self);
  const gchar *str;
  GObject *obj;

  switch (property_id) {
    case PROP_AVATR:
      obj = g_value_get_object (value);
      if (gtk_image_get_pixbuf (GTK_IMAGE (priv->avatar)) != GDK_PIXBUF (obj)) {
        gtk_image_set_from_pixbuf (GTK_IMAGE (priv->avatar), GDK_PIXBUF (obj));
      }
      break;

    case PROP_NAME:
      str = g_value_get_string (value);
      gtk_label_set_markup (GTK_LABEL (priv->name), str);
      gtk_widget_set_visible (priv->name, !(str == NULL || *str == '\0'));
      break;

    case PROP_DESCRIPTION:
      str = g_value_get_string (value);
      gtk_label_set_markup (GTK_LABEL (priv->description), str);
      gtk_widget_set_visible (priv->description, !(str == NULL || *str == '\0'));
      break;

    case PROP_TIMESTAMP:
      str = g_value_get_string (value);
      gtk_label_set_markup (GTK_LABEL (priv->timestamp), str);
      gtk_widget_set_visible (priv->timestamp, !(str == NULL || *str == '\0'));
      break;

    case PROP_MESSAGE_COUNT:
      str = g_value_get_string (value);
      gtk_label_set_markup (GTK_LABEL (priv->message_count), str);
      gtk_widget_set_visible (priv->message_count, !(str == NULL || *str == '\0'));
      break;

    case PROP_DATA:
      priv->data = g_value_get_pointer (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}


static void
chatty_contact_row_class_init (ChattyContactRowClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = chatty_contact_row_set_property;
  object_class->get_property = chatty_contact_row_get_property;

  props[PROP_AVATR] =
   g_param_spec_object ("avatar",
                        "Contact Avatar",
                        "The contact avatar in the row",
                        GDK_TYPE_PIXBUF,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_NAME] =
   g_param_spec_string ("name",
                        "Contact name",
                        "The name of a contact",
                        "",
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_DESCRIPTION] =
   g_param_spec_string ("description",
                        "Description",
                        "A description for the contact, but it can be used also for the last message",
                        "",
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_TIMESTAMP] =
   g_param_spec_string ("timestamp",
                        "Timestamp",
                        "A timestamp for the last message send my the contact",
                        "",
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);


  props[PROP_MESSAGE_COUNT] =
   g_param_spec_string ("message_count",
                        "Message Count",
                        "Number of unread messages",
                        "",
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_DATA] =
   g_param_spec_pointer ("data",
                        "Data",
                        "Data normaly used to keep track of the PurpleBlistNode",
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);


  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/chatty/ui/chatty-contact-row.ui");
  gtk_widget_class_bind_template_child_private (widget_class, ChattyContactRow, avatar);
  gtk_widget_class_bind_template_child_private (widget_class, ChattyContactRow, name);
  gtk_widget_class_bind_template_child_private (widget_class, ChattyContactRow, description);
  gtk_widget_class_bind_template_child_private (widget_class, ChattyContactRow, timestamp);
  gtk_widget_class_bind_template_child_private (widget_class, ChattyContactRow, message_count);
}

static void
chatty_contact_row_init (ChattyContactRow *self) {
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
chatty_contact_row_new (gpointer data,
                        GdkPixbuf *avatar,
                        const gchar *name,
                        const gchar *description,
                        const gchar *timestamp,
                        const gchar *message_count) {
  return g_object_new (CHATTY_TYPE_CONTACT_ROW,
                       "data", data,
                       "avatar", avatar,
                       "name", name,
                       "description", description, 
                       "timestamp", timestamp,
                       "message_count", message_count,
                       NULL);
}
