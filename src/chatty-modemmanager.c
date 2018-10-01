/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */


#include <stdbool.h>
#include <stdio.h>
#include <glib/gprintf.h>
#include <gio/gio.h>
#include "chatty-modemmanager.h"
#include "chatty-message-list.h"

static gboolean chatty_mm_retrieve_sms (const gchar *sms_path);

static chatty_mm_data_t chatty_mm_data;

chatty_mm_data_t *chatty_get_mm_data (void)
{
  return &chatty_mm_data;
}


static void
cb_messaging_proxy (GDBusProxy   *proxy,
                    const gchar  *sender_name,
                    const gchar  *signal_name,
                    GVariant     *parameters,
                    gpointer     data)
{
  gchar     *sms_path;
  gboolean  status_flag;
  gint      old_state, new_state;
  guint     change_reason;

  chatty_mm_data_t *chatty_mm = chatty_get_mm_data();

  if (g_str_equal (signal_name, "Added")) {
    g_variant_get(parameters, "(ob)", &sms_path, &status_flag);

    if (status_flag)  {
      chatty_mm->sms_path = g_list_prepend (chatty_mm->sms_path,
                                            g_strdup (sms_path));
    }
  } else if (g_str_equal(signal_name, "StateChanged")) {
    g_variant_get(parameters, "(iiu)", &old_state, &new_state, &change_reason);
  }

  printf ("Signal: %s (%s) type: %s\n", signal_name, sender_name, sms_path);

  chatty_mm_retrieve_sms (sms_path);
}


static void
cb_send_sms (GDBusProxy   *proxy,
             GAsyncResult *res,
             gpointer     user_data)
{
  GError      *error;
  gboolean    sent;
  gchar       *smspath;

  if (user_data == NULL)
    return;

  chatty_mm_data_t *chatty_mm = chatty_get_mm_data();

  error = NULL;

  g_dbus_proxy_call_finish (proxy, res, &error);

  if (error != NULL) {
    printf ("SMS send error: %s\n", error->message);
    g_error_free (error);
    sent = FALSE;
  } else {
    sent = TRUE;
  }

  smspath = g_dbus_proxy_get_object_path(proxy);

  if (smspath != NULL) {
    error = NULL;

    g_dbus_proxy_call_sync (chatty_mm->proxy_messaging,
                            "Delete",
                            g_variant_new("(o)", smspath),
                            0,
                            -1,
                            NULL,
                            &error);

    if (error != NULL) {
      g_error_free(error);
    }
  }
}


static gboolean
chatty_mm_retrieve_sms (const gchar *sms_path)
{
  GError      *error;
  GDBusProxy  *proxy_sms;
  GVariant    *value;
  gsize       str_len;
  guint       state;
  gchar       *valuestr;

  error = NULL;

  chatty_mm_data_t *chatty_mm = chatty_get_mm_data();

  proxy_sms = g_dbus_proxy_new_sync (chatty_mm->connection,
                  G_DBUS_PROXY_FLAGS_NONE,
                  NULL,
                  "org.freedesktop.ModemManager1",
                  sms_path,
                  "org.freedesktop.ModemManager1.Sms",
                  NULL,
                  &error);

  if ((proxy_sms == NULL) && (error != NULL)) {
    g_error_free (error);
    return FALSE;
  }

  value  = g_dbus_proxy_get_cached_property (proxy_sms, "State");

  if (value != NULL) {
    state = g_variant_get_uint32 (value);
    printf ("STATE: %u\n", state);

    if (state != MODULE_SMS_STATE_RECEIVED) {
      g_variant_unref (value);
      return FALSE;
    }
    g_variant_unref (value);
  } else {
    return FALSE;
  }

  value  = g_dbus_proxy_get_cached_property (proxy_sms, "PduType");

  if (value != NULL) {
    state = g_variant_get_uint32 (value);
    printf ("PDU: %u\n", state);

    if ((state == MODULE_SMS_PDU_TYPE_UNKNOWN) ||
        (state == MODULE_SMS_PDU_TYPE_SUBMIT)) {
      g_variant_unref (value);
      return FALSE;
    }

    g_variant_unref(value);
  } else {
    return FALSE;
  }

  value  = g_dbus_proxy_get_cached_property (proxy_sms, "Number");

  if (value != NULL) {
    str_len = 256;
    valuestr = g_variant_get_string (value, &str_len);

    if ((valuestr != NULL) && (valuestr[0] != '\0')) {
      printf ("SMS number: %s\n", valuestr);
    } else {
      printf ("SMS number unknown\n");
    }

    g_variant_unref(value);
  } else {
    printf ("SMS number unknown\n");
  }

  value = g_dbus_proxy_get_cached_property (proxy_sms, "Text");

  if (value != NULL) {
    str_len = 256 * 160;
    valuestr = g_variant_get_string (value, &str_len);

    if ((valuestr != NULL) && (valuestr[0] != '\0')) {
      printf ("SMS text: %s\n", valuestr);
    } else {
      g_variant_unref(value);
      return FALSE;
    }

    g_variant_unref(value);
  }

  value = g_dbus_proxy_get_cached_property (proxy_sms, "Timestamp");

  if (value != NULL) {
    str_len = 256;
    valuestr = g_variant_get_string (value, &str_len);

    if ((valuestr != NULL) && (valuestr[0] != '\0')) {
      printf ("SMS timestamp: %s\n", ctime((const time_t *)&valuestr));
    }

    g_variant_unref (value);
  }

  g_object_unref (proxy_sms);

  return TRUE;
}


gboolean
chatty_mm_send_sms (gchar*    number,
                    gchar     *text,
                    gboolean  report,
                    guint     validity)
{
  GError          *error;
  GVariantBuilder *builder;
  GVariant        *array;
  GVariant        *message;
  GVariant        *sms_path_v;
  gboolean        sent;
 gchar            *sms_path;
  GDBusProxy      *proxy_sms;

  if ((number == NULL) || (text == NULL))
    return FALSE;

  chatty_mm_data_t *chatty_mm = chatty_get_mm_data();

  if (chatty_mm->proxy_messaging == NULL)
    return FALSE;

  builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);

  g_variant_builder_add_parsed (builder, "{'number', <%s>}", number);
  g_variant_builder_add_parsed (builder, "{'text', <%s>}", text);

  if ((validity > -1) && (validity <= 255)) {
    g_variant_builder_add_parsed (builder, "{'validity', %v}",
                                  g_variant_new ("(uv)",
                                  1,
                                  g_variant_new_uint32 ((guint) validity)));
  }

  g_variant_builder_add_parsed (builder,
                                "{'delivery-report-request', <%b>}",
                                report);

  array = g_variant_builder_end (builder);

  builder = g_variant_builder_new (G_VARIANT_TYPE_TUPLE);
  g_variant_builder_add_value (builder, array);
  message = g_variant_builder_end (builder);

  error = NULL;

  sms_path_v = g_dbus_proxy_call_sync (chatty_mm->proxy_messaging,
                                      "Create",
                                      message,
                                      0,
                                      -1,
                                      NULL,
                                      &error);

  if ((sms_path_v == NULL) && (error != NULL)) {
    printf ("SMS create error: %s\n", error->message);
    g_error_free (error);
    return FALSE;
  }

  g_variant_get(sms_path_v, "(o)", &sms_path);

  if (sms_path == NULL) {
    printf ("Failed to obtain object path for saved SMS message\n");
    return FALSE;
  }

  error = NULL;

  proxy_sms = g_dbus_proxy_new_sync (chatty_mm->connection,
                                      G_DBUS_PROXY_FLAGS_NONE,
                                      NULL,
                                      "org.freedesktop.ModemManager1",
                                      sms_path,
                                      "org.freedesktop.ModemManager1.Sms",
                                      NULL,
                                      &error);

  if ((proxy_sms == NULL) && (error != NULL)) {
    printf ("SMS proxy error: %s\n", error->message);
    g_error_free (error);
    g_free (sms_path);
    return FALSE;
  }

  g_free (sms_path);

  g_dbus_proxy_call (proxy_sms,
                     "Send",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     MM_SEND_SMS_TIMEOUT,
                     NULL,
                     (GAsyncReadyCallback) cb_send_sms,
                     0);

  return TRUE;
}


static
chatty_mm_get_properties (void)
{
  GVariant  *device_info;
  gsize     str_size;

  chatty_mm_data_t *chatty_mm = chatty_get_mm_data();

  device_info = g_dbus_proxy_get_cached_property (chatty_mm->proxy_device, "Manufacturer");
  if (device_info != NULL) {
    str_size = 256;
    chatty_mm->manufacturer = g_strdup (g_variant_get_string (device_info, &str_size));
    g_variant_unref (device_info);
  } else {
    chatty_mm->manufacturer = g_strdup ("n/a");
  }

  device_info = g_dbus_proxy_get_cached_property (chatty_mm->proxy_device, "Model");
  if (device_info != NULL) {
    str_size = 256;
    chatty_mm->model = g_strdup (g_variant_get_string (device_info, &str_size));
    g_variant_unref (device_info);
  } else {
    chatty_mm->model = g_strdup ("n/a");
  }

  device_info = g_dbus_proxy_get_cached_property (chatty_mm->proxy_device, "Revision");
  if (device_info != NULL) {
    str_size = 256;
    chatty_mm->version = g_strdup (g_variant_get_string (device_info, &str_size));
    g_variant_unref(device_info);
  } else {
    chatty_mm->version = g_strdup ("n/a");
  }

  device_info = g_dbus_proxy_get_cached_property (chatty_mm->proxy_device, "State");
  if (device_info != NULL) {
    chatty_mm->device_state = g_variant_get_int32 (device_info);
    chatty_mm->device_enabled = chatty_mm->device_state & MM_MODEM_STATE_ENABLED;
    chatty_mm->device_registered = chatty_mm->device_state & MM_MODEM_STATE_REGISTERED;
    g_variant_unref (device_info);
  } else {
    chatty_mm->device_enabled = FALSE;
    chatty_mm->device_registered =  FALSE;
  }

  printf ("Manufacturer: %s\n", chatty_mm->manufacturer);
  printf ("Model: %s\n", chatty_mm->model);
  printf ("Version: %s\n", chatty_mm->version);
  printf ("State: %i\n", chatty_mm->device_state);
}


static void
chatty_mm_get_proxies (void)
{
  GError *error;

  chatty_mm_data_t *chatty_mm = chatty_get_mm_data();

  if (chatty_mm->connection == NULL) return;

  error = NULL;

  chatty_mm->proxy_device =
    g_dbus_proxy_new_sync (chatty_mm->connection,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.freedesktop.ModemManager1",
                            chatty_mm->device_path,
                            "org.freedesktop.ModemManager1.Modem",
                            NULL,
                            &error);

  if ((chatty_mm->proxy_device == NULL) && (error != NULL)) {
    printf ("Device proxy error: %s\n", error->message);
    g_error_free (error);
    g_object_unref (chatty_mm->proxy_device);
    chatty_mm->manufacturer = g_strdup ("n/a");
    chatty_mm->model = g_strdup ("n/a");
    chatty_mm->version = g_strdup ("n/a");
  } else {
    g_signal_connect (chatty_mm->proxy_device,
                      "g-signal",
                      G_CALLBACK (cb_messaging_proxy),
                      0);
  }

  chatty_mm->proxy_messaging =
    g_dbus_proxy_new_sync (chatty_mm->connection,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.freedesktop.ModemManager1",
                            chatty_mm->device_path,
                            "org.freedesktop.ModemManager1.Modem.Messaging",
                            NULL,
                            &error);

  if ((chatty_mm->proxy_messaging == NULL) && (error != NULL)) {
    printf ("Messaging proxy error: %s\n", error->message);
    g_error_free (error);
    g_object_unref (chatty_mm->proxy_messaging);
  } else {
    g_signal_connect (chatty_mm->proxy_messaging,
                      "g-signal",
                      G_CALLBACK (cb_messaging_proxy),
                      0);
  }

  chatty_mm->sms_path = NULL;
}


static void
chatty_mm_enable_device (gboolean enabled)
{
  GError   *error;

  error = NULL;

  chatty_mm_data_t *chatty_mm = chatty_get_mm_data();

  g_dbus_proxy_call_sync (chatty_mm->proxy_device,
                          "Enable",
                          g_variant_new("(b)", enabled),
                          G_DBUS_CALL_FLAGS_NONE,
                          MM_SETTING_TIMEOUT,
                          NULL,
                          &error);

  if (error != NULL) {
    // TODO err handling
    printf ("Device enable error: %s\n", error->message);
    g_error_free (error);
  }
}


gboolean
chatty_mm_open_device (void)
{
  GError *error = NULL;
  GList  *object;
  GList  *objects;

  chatty_mm_data_t *chatty_mm = chatty_get_mm_data();

  chatty_mm->connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);

  if ((chatty_mm->connection == NULL) && (error != NULL)) {
    printf ("Connection error: %s\n", error->message);
    g_error_free (error);
    g_object_unref (chatty_mm->connection);
    return FALSE;
  }

  chatty_mm->objectmanager =
    g_dbus_object_manager_client_new_sync (chatty_mm->connection,
                                            G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                                            "org.freedesktop.ModemManager1",
                                            "/org/freedesktop/ModemManager1",
                                            NULL,
                                            NULL,
                                            NULL,
                                            NULL,
                                            &error);

  if ((chatty_mm->objectmanager == NULL) && (error != NULL)) {
    printf ("Device open error: %s\n", error->message);
    g_error_free (error);
    g_object_unref (chatty_mm->connection);
    return FALSE;
  }

  objects = g_dbus_object_manager_get_objects (chatty_mm->objectmanager);

  for (object = objects; object != NULL; object = object->next) {
    chatty_mm->device_path = g_dbus_object_get_object_path (G_DBUS_OBJECT(object->data));
  }

  g_list_foreach (objects, (GFunc) g_object_unref, NULL);
  g_list_free (objects);

  printf ("Device object path: %s\n", chatty_mm->device_path);

  chatty_mm_get_proxies ();
  chatty_mm_enable_device (TRUE);
  chatty_mm_get_properties ();

  return TRUE;
}


void
chatty_mm_close_device (void)
{
  chatty_mm_data_t *chatty_mm = chatty_get_mm_data();

  if (chatty_mm->objectmanager != NULL) {
    g_object_unref(chatty_mm->objectmanager);
    chatty_mm->objectmanager = NULL;
  }

  if (chatty_mm->connection != NULL) {
    g_object_unref(chatty_mm->connection);
    chatty_mm->connection = NULL;
  }

  if (chatty_mm->proxy_device != NULL) {
    g_object_unref (chatty_mm->proxy_device);
    chatty_mm->proxy_device = NULL;
  }

  if (chatty_mm->proxy_messaging != NULL) {
    g_object_unref (chatty_mm->proxy_messaging);
    chatty_mm->proxy_messaging = NULL;
  }
}
