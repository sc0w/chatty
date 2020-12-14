/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-secret-store.c
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-secret-store"


#include <libsecret/secret.h>
#include <glib/gi18n.h>

#include "matrix/matrix-utils.h"
#include "matrix/chatty-ma-account.h"
#include "chatty-secret-store.h"

#define PROTOCOL_MATRIX_STR  "matrix"

static const SecretSchema *
secret_store_get_schema (void)
{
  static const SecretSchema password_schema = {
    "sm.puri.Chatty", SECRET_SCHEMA_NONE,
    {
      { CHATTY_USERNAME_ATTRIBUTE,  SECRET_SCHEMA_ATTRIBUTE_STRING },
      { CHATTY_SERVER_ATTRIBUTE,    SECRET_SCHEMA_ATTRIBUTE_STRING },
      { CHATTY_PROTOCOL_ATTRIBUTE,  SECRET_SCHEMA_ATTRIBUTE_STRING },
      { NULL, 0 },
    }
  };

  return &password_schema;
}

void
chatty_secret_store_save_async (ChattyAccount       *account,
                                char                *access_token,
                                const char          *device_id,
                                char                *pickle_key,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  const SecretSchema *schema;
  GHashTable *attr;
  g_autofree char *label = NULL;
  const char *server, *old_pass;
  char *password = NULL, *token = NULL, *key = NULL;
  char *credentials;

  /* Currently we support matrix accounts only  */
  g_return_if_fail (CHATTY_IS_MA_ACCOUNT (account));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  old_pass = chatty_account_get_password (account);

  if (old_pass && *old_pass)
    password = g_strescape (old_pass, NULL);
  if (access_token && *access_token)
    token = g_strescape (access_token, NULL);
  if (pickle_key && *pickle_key)
    key = g_strescape (pickle_key, NULL);

  if (!device_id)
    device_id = "";

  /* We don't use json APIs here so that we can manage memory better (and securely free them)  */
  /* TODO: Use a non-pageable memory */
  /* XXX: We use a dumb string search, so don't change the order or spacing of the format string */
  credentials = g_strdup_printf ("{\"password\": \"%s\", \"access-token\": \"%s\", "
                                 "\"pickle-key\": \"%s\", \"device-id\": \"%s\"}",
                                 password ? password : "", token ? token : "",
                                 key ? key : "", device_id);
  schema = secret_store_get_schema ();
  server = chatty_ma_account_get_homeserver (CHATTY_MA_ACCOUNT (account));
  label = g_strdup_printf (_("Chatty password for \"%s\""), chatty_account_get_username (account));
  attr = secret_attributes_build (schema,
                                  CHATTY_USERNAME_ATTRIBUTE, chatty_account_get_username (account),
                                  CHATTY_SERVER_ATTRIBUTE, server,
                                  CHATTY_PROTOCOL_ATTRIBUTE, PROTOCOL_MATRIX_STR,
                                  NULL);
  secret_password_storev (schema, attr, NULL, label, credentials,
                          cancellable, callback, user_data);

  matrix_utils_free_buffer (access_token);
  matrix_utils_free_buffer (credentials);
  matrix_utils_free_buffer (pickle_key);
  matrix_utils_free_buffer (password);
  matrix_utils_free_buffer (token);
  matrix_utils_free_buffer (key);
}

gboolean
chatty_secret_store_save_finish (GAsyncResult  *result,
                                 GError       **error)
{
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return secret_password_store_finish (result, error);
}

void
chatty_secret_delete_async (ChattyAccount       *account,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  const SecretSchema *schema;
  g_autoptr(GHashTable) attr = NULL;
  const char *server;

  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  schema = secret_store_get_schema ();
  server = chatty_ma_account_get_homeserver (CHATTY_MA_ACCOUNT (account));
  attr = secret_attributes_build (schema,
                                  CHATTY_USERNAME_ATTRIBUTE, chatty_account_get_username (account),
                                  CHATTY_SERVER_ATTRIBUTE, server,
                                  CHATTY_PROTOCOL_ATTRIBUTE, PROTOCOL_MATRIX_STR,
                                  NULL);
  secret_service_clear (NULL, schema, attr, cancellable, callback, user_data);
}

gboolean
chatty_secret_delete_finish  (GAsyncResult  *result,
                              GError       **error)
{
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return secret_service_clear_finish (NULL, result, error);
}
