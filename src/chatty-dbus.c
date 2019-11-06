/*
 * Copyright (C) 2019 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-gc-dbus"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "chatty-dbus.h"


typedef struct {
  GDBusConnection *connection;
  GCancellable    *cancellable;
  char            *name;
  char            *number;
} DbusData;

static void chatty_dbus_write_contact_stop (DbusData *data);



static void
cb_bus_connected (GObject      *source_object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  GDBusConnection  *connection;
  GDBusActionGroup *action_group;
  DbusData         *data = user_data;
  GVariant         *contact;
  GVariant         *array;
  GVariantBuilder  *builder;

  g_autoptr(GError) error = NULL;

  connection = g_bus_get_finish (res, &error);

  if (connection == NULL && error != NULL) {
    g_error ("%s Could not get dbus connection: %s", __func__, error->message);
    g_object_unref (connection);
    return;
  }

  data->connection = connection;

  builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
  g_variant_builder_add_parsed (builder, "{'full-name', <%s>}", data->name);
  g_variant_builder_add_parsed (builder, "{'phone-numbers', <%s>}", data->number);
  array = g_variant_builder_end (builder);

  builder = g_variant_builder_new (G_VARIANT_TYPE_TUPLE);
  g_variant_builder_add_value (builder, array);
  contact = g_variant_builder_end (builder);

  action_group = g_dbus_action_group_get (connection,
                                          "org.gnome.Contacts",
                                          "/org/gnome/Contacts");

  g_action_group_activate_action ((GActionGroup*)action_group,
                                  "new-contact-data",
                                  contact);

  chatty_dbus_write_contact_stop (data);
}


static void
chatty_dbus_write_contact_stop (DbusData *data)
{
  g_clear_object (&data->connection);
  g_clear_object (&data->cancellable);

  g_free (data->name);
  g_free (data->number);

  g_slice_free (DbusData, data);
}


void
chatty_dbus_gc_write_contact (const char *contact_name,
                              const char *phone_number)
{
  DbusData *data;

  data = g_slice_new0 (DbusData);

  data->cancellable = g_cancellable_new ();
  data->name = g_strdup (contact_name);
  data->number = g_strdup (phone_number);

  g_bus_get (G_BUS_TYPE_SESSION,
             data->cancellable,
             (GAsyncReadyCallback) cb_bus_connected,
             data);
}