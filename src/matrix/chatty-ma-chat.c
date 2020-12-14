/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-ma-chat.c
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-ma-chat"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>

#include "contrib/gtk.h"
#include "chatty-config.h"
#include "chatty-history.h"
#include "chatty-utils.h"
#include "matrix-api.h"
#include "matrix-db.h"
#include "matrix-enc.h"
#include "matrix-utils.h"
#include "chatty-ma-buddy.h"
#include "chatty-ma-chat.h"

#define CHATTY_COLOR_BLUE "4A8FD9"

/**
 * SECTION: chatty-chat
 * @title: ChattyChat
 * @short_description: An abstraction over #PurpleConversation
 * @include: "chatty-chat.h"
 *
 * libpurple doesn’t have a nice OOP interface for managing anything.
 * This class hides all the complexities surrounding it.
 */

struct _ChattyMaChat
{
  ChattyChat           parent_instance;

  char                *room_name;
  char                *generated_name;
  char                *room_id;
  char                *encryption;
  char                *prev_batch;
  char                *last_batch;
  ChattyMaBuddy       *self_buddy;
  GListStore          *buddy_list;
  GListStore          *message_list;
  GtkSortListModel    *sorted_message_list;

  /* Pending messages to be sent.  Queue messages here when
     @self is busy (eg: claiming keys for encrypted chat) */
  GQueue              *message_queue;

  JsonObject       *json_data;
  ChattyAccount    *account;
  MatrixApi        *matrix_api;
  MatrixEnc        *matrix_enc;
  MatrixDb         *matrix_db;
  ChattyHistory    *history_db;

  gint64          highlight_count;
  int             unread_count;
  int             room_name_update_ts;

  guint          state_is_sync    : 1;
  guint          state_is_syncing : 1;
  /* Set if the complete buddy list is loaded */
  guint          claiming_keys : 1;
  guint          keys_claimed : 1;
  guint          prev_batch_loading : 1;

  guint          room_name_loaded : 1;
  guint          buddy_typing : 1;
  /* Set if server says we are typing */
  guint          self_typing : 1;
  /* The time when self_typing was updated */
  /* Uses g_get_monotonic_time(), we only need the interval */
  gint64         self_typing_set_time;
};

G_DEFINE_TYPE (ChattyMaChat, chatty_ma_chat, CHATTY_TYPE_CHAT)

enum {
  PROP_0,
  PROP_JSON_DATA,
  PROP_ROOM_ID,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static int
sort_message (gconstpointer a,
              gconstpointer b,
              gpointer      user_data)
{
  time_t time_a, time_b;

  time_a = chatty_message_get_time ((gpointer)a);
  time_b = chatty_message_get_time ((gpointer)b);

  return time_a - time_b;
}

static void
chatty_mat_chat_update_name (ChattyMaChat *self)
{
  g_autoptr(ChattyMaBuddy) buddy_b = NULL;
  const char *name_a = NULL, *name_b = NULL;
  guint n_items, count;

  g_assert (CHATTY_IS_MA_CHAT (self));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->buddy_list));

  if (self->room_name || n_items == 0)
    return;

  count = n_items;

  for (guint i = 0; i < MIN (3, n_items); i++) {
    g_autoptr(ChattyItem) buddy = NULL;

    buddy = g_list_model_get_item (G_LIST_MODEL (self->buddy_list), i);

    /* Don't add self to create room name */
    if (g_strcmp0 (chatty_ma_buddy_get_id (CHATTY_MA_BUDDY (buddy)), chatty_account_get_username (self->account)) == 0) {
      count--;
      continue;
    }

    if (!name_a)
      name_a = chatty_item_get_name (buddy);
    else
      name_b = chatty_item_get_name (buddy);
  }

  g_free (self->generated_name);

  if (count == 0)
    self->generated_name = g_strdup (_("Empty room"));
  else if (count == 1)
    self->generated_name = g_strdup (name_a);
  else if (count == 2)
    self->generated_name = g_strdup_printf (_("%s and %s"), name_a, name_b);
  else
    self->generated_name = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%s and %u other",
                                                         "%s and %u others", count - 1),
                                            name_a, count - 1);
  g_signal_emit_by_name (self, "avatar-changed");
}

static ChattyMaBuddy *
ma_chat_find_buddy (ChattyMaChat *self,
                    GListModel   *model,
                    const char   *matrix_id,
                    guint        *index)
{
  guint n_items;

  g_assert (CHATTY_IS_MA_CHAT (self));
  g_assert (G_IS_LIST_MODEL (model));
  g_return_val_if_fail (matrix_id && *matrix_id, NULL);

  n_items = g_list_model_get_n_items (model);
  for (guint i = 0; i < n_items; i++) {
    g_autoptr(ChattyMaBuddy) buddy = NULL;

    buddy = g_list_model_get_item (model, i);
    if (g_str_equal (chatty_ma_buddy_get_id (buddy), matrix_id)) {
      if (index)
        *index = i;

      return buddy;
    }
  }

  return NULL;
}

static ChattyMaBuddy *
ma_chat_add_buddy (ChattyMaChat *self,
                   GListStore   *store,
                   const char   *matrix_id)
{
  g_autoptr(ChattyMaBuddy) buddy = NULL;

  g_assert (CHATTY_IS_MA_CHAT (self));

  if (!matrix_id || *matrix_id != '@')
    g_return_val_if_reached (NULL);

  buddy = chatty_ma_buddy_new (matrix_id,
                               self->matrix_api,
                               self->matrix_enc);
  g_list_store_append (store, buddy);

  return buddy;
}

static void
handle_m_room_member (ChattyMaChat *self,
                      JsonObject   *object,
                      GListStore   *members_list)
{
  GListModel *model;
  ChattyMaBuddy *buddy;
  JsonObject *content;
  const char *value, *membership, *sender, *name;

  g_assert (CHATTY_IS_MA_CHAT (self));
  g_assert (object);
  g_assert (G_IS_LIST_STORE (members_list));

  value = matrix_utils_json_object_get_string (object, "room_id");
  if (g_strcmp0 (value, self->room_id) != 0) {
    g_warning ("room_id '%s' doesn't match '%s", value, self->room_id);
    return;
  }

  sender = matrix_utils_json_object_get_string (object, "sender");
  content = matrix_utils_json_object_get_object (object, "content");
  membership = matrix_utils_json_object_get_string (content, "membership");

  model = G_LIST_MODEL (members_list);
  buddy = ma_chat_find_buddy (self, model, sender, NULL);

  if (!buddy && members_list != self->buddy_list) {
    model = G_LIST_MODEL (self->buddy_list);
    buddy = ma_chat_find_buddy (self, model, sender, NULL);
  }

  name = matrix_utils_json_object_get_string (content, "displayname");
  if (buddy)
    chatty_item_set_name (CHATTY_ITEM (buddy), name);

  if (g_strcmp0 (membership, "join") == 0) {
    if (buddy && model == (gpointer)members_list)
      return;

    if (buddy)
      g_list_store_append (members_list, buddy);
    else
      buddy = ma_chat_add_buddy (self, members_list, sender);
    chatty_item_set_name (CHATTY_ITEM (buddy), name);
  } else if (buddy &&
             g_strcmp0 (membership, "leave") == 0) {
    self->keys_claimed = FALSE;
    chatty_utils_remove_list_item (members_list, buddy);
    g_clear_pointer (&self->generated_name, g_free);
  }
}

static void
handle_m_room_name (ChattyMaChat *self,
                    JsonObject   *root)
{
  JsonObject *content;
  const char *name;

  g_assert (CHATTY_MA_CHAT (self));

  content = matrix_utils_json_object_get_object (root, "content");
  name = matrix_utils_json_object_get_string (content, "name");

  if (name && g_strcmp0 (name, self->room_name) != 0) {
    g_free (self->room_name);
    self->room_name = g_strdup (name);
    g_object_notify (G_OBJECT (self), "name");
    g_signal_emit_by_name (self, "avatar-changed");
  }
}

static void
handle_m_room_encryption (ChattyMaChat *self,
                          JsonObject   *root)
{
  JsonObject *content;

  g_assert (CHATTY_IS_MA_CHAT (self));

  if (!root || self->encryption)
    return;

  content = matrix_utils_json_object_get_object (root, "content");
  self->encryption = g_strdup (matrix_utils_json_object_get_string (content, "algorithm"));

  if (self->encryption)
    g_object_notify (G_OBJECT (self), "encrypt");
}

static void
ma_chat_download_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  g_autoptr(GError) error = NULL;
  ChattyMaChat *self = user_data;
  GTask *task = G_TASK (result);
  ChattyMessage *message;
  ChattyFileInfo *file;

  g_assert (CHATTY_IS_MA_CHAT (self));

  file = g_object_get_data (G_OBJECT (task), "file");
  message = g_object_get_data (G_OBJECT (task), "message");
  g_return_if_fail (file);
  g_return_if_fail (message);

  if (matrix_api_get_file_finish (self->matrix_api, result, &error))
    file->status = CHATTY_FILE_DOWNLOADED;

  chatty_history_add_message (self->history_db, CHATTY_CHAT (self), message);
}

static void
ma_chat_parse_base64_value (guchar     **out,
                            gsize       *out_len,
                            const char  *value)
{
  g_autofree char *base64 = NULL;
  gsize len, padded_len;

  g_assert (out);
  g_assert (out_len);

  if (!value)
    return;

  len = strlen (value);
  /* base64 is always multiple of 4, so add space for padding */
  if (len % 4)
    padded_len = len + 4 - len % 4;
  else
    padded_len = len;
  base64 = malloc (padded_len + 1);
  strcpy (base64, value);
  memset (base64 + len, '=', padded_len - len);
  base64[padded_len] = '\0';

  *out = g_base64_decode (base64, out_len);
}

static void
chat_handle_m_media (ChattyMaChat  *self,
                     ChattyMessage *message,
                     JsonObject    *content,
                     const char    *type,
                     gboolean       encrypted)
{
  ChattyFileInfo *file = NULL;
  JsonObject *object;
  const char *url;

  g_assert (CHATTY_IS_MA_CHAT (self));
  g_assert (CHATTY_IS_MESSAGE (message));
  g_assert (content);
  g_assert (type);

  if (encrypted)
    object = matrix_utils_json_object_get_object (content, "file");
  else
    object = content;

  if (!matrix_utils_json_object_get_string (object, "url"))
    return;

  if (!g_str_equal (type, "m.image") &&
      !g_str_equal (type, "m.video") &&
      !g_str_equal (type, "m.file") &&
      !g_str_equal (type, "m.audio"))
    return;

  url = matrix_utils_json_object_get_string (object, "url");

  if (url && g_str_has_prefix (url, "mxc://")) {
    file = g_new0 (ChattyFileInfo, 1);

    url = url + strlen ("mxc://");
    file->url = g_strconcat (matrix_api_get_homeserver (self->matrix_api),
                             "/_matrix/media/r0/download/", url, NULL);
    file->file_name = g_strdup (matrix_utils_json_object_get_string (object, "body"));
    object = matrix_utils_json_object_get_object (content, "info");
    file->mime_type = g_strdup (matrix_utils_json_object_get_string (object, "mimetype"));
    file->height = matrix_utils_json_object_get_int (object, "h");
    file->width = matrix_utils_json_object_get_int (object, "w");
    file->size = matrix_utils_json_object_get_int (object, "size");
  }

  if (encrypted && file) {
    g_autoptr(MatrixFileEncInfo) info = NULL;
    JsonObject *json_key;

    object = matrix_utils_json_object_get_object (content, "file");
    json_key = matrix_utils_json_object_get_object (object, "key");

    if (g_strcmp0 (matrix_utils_json_object_get_string (object, "v"), "v2") != 0 ||
        g_strcmp0 (matrix_utils_json_object_get_string (json_key, "alg"), "A256CTR") != 0 ||
        !matrix_utils_json_object_get_bool (json_key, "ext") ||
        g_strcmp0 (matrix_utils_json_object_get_string (json_key, "kty"), "oct") != 0)
      return;

    info = g_new0 (MatrixFileEncInfo, 1);
    info->aes_iv_base64 = g_strdup (matrix_utils_json_object_get_string (object, "iv"));
    info->aes_key_base64 = g_strdup (matrix_utils_json_object_get_string (json_key, "k"));
    /* XXX: update doc: uses basae64url */
    g_strdelimit (info->aes_key_base64, "_", '/');
    g_strdelimit (info->aes_key_base64, "-", '+');

    object = matrix_utils_json_object_get_object (object, "hashes");
    info->sha256_base64 = g_strdup (matrix_utils_json_object_get_string (object, "sha256"));

    ma_chat_parse_base64_value (&info->aes_iv, &info->aes_iv_len, info->aes_iv_base64);
    ma_chat_parse_base64_value (&info->aes_key, &info->aes_key_len, info->aes_key_base64);
    ma_chat_parse_base64_value (&info->sha256, &info->sha256_len, info->sha256_base64);

    if (info->aes_iv_len == 16 && info->aes_key_len == 32 && info->sha256_len == 32)
      file->user_data = g_steal_pointer (&info);

    if (file->user_data)
      matrix_db_save_file_url_async (self->matrix_db, message, file, 2,
                                     CHATTY_ALGORITHM_A256CTR, CHATTY_KEY_TYPE_OCT, TRUE,
                                     NULL, NULL);
  }

  if (!file)
    return;

  g_object_set_data_full (G_OBJECT (message), "file-url", g_strdup (file->url), g_free);
  matrix_api_get_file_async (self->matrix_api, message, file, NULL, NULL,
                             ma_chat_download_cb, self);
  chatty_message_set_file (message, file);
}

static void
matrix_add_message_from_data (ChattyMaChat  *self,
                              ChattyMaBuddy *buddy,
                              JsonObject    *root,
                              JsonObject    *object,
                              gboolean       encrypted)
{
  g_autoptr(ChattyMessage) message = NULL;
  g_autofree char *value = NULL;
  JsonObject *content;
  const char *body, *type;
  ChattyMsgDirection direction = CHATTY_DIRECTION_IN;
  ChattyMsgType msg_type;
  const char *uuid;
  time_t ts;

  g_assert (CHATTY_IS_MA_CHAT (self));
  g_assert (object);

  content = matrix_utils_json_object_get_object (object, "content");
  type = matrix_utils_json_object_get_string (content, "msgtype");

  if (!type)
    return;

  if (g_str_equal (type, "m.image"))
    msg_type = CHATTY_MESSAGE_IMAGE;
  else if (g_str_equal (type, "m.video"))
    msg_type = CHATTY_MESSAGE_VIDEO;
  else if (g_str_equal (type, "m.file"))
    msg_type = CHATTY_MESSAGE_FILE;
  else if (g_str_equal (type, "m.audio"))
    msg_type = CHATTY_MESSAGE_AUDIO;
  else if (g_str_equal (type, "m.location"))
    msg_type = CHATTY_MESSAGE_LOCATION;
  else
    msg_type = CHATTY_MESSAGE_TEXT;

  body = matrix_utils_json_object_get_string (content, "body");
  if (root)
    uuid = matrix_utils_json_object_get_string (root, "event_id");
  else
    uuid = matrix_utils_json_object_get_string (object, "event_id");

  /* timestamp is in milliseconds */
  ts = matrix_utils_json_object_get_int (object, "origin_server_ts");
  ts = ts / 1000;

  if (buddy == self->self_buddy)
    direction = CHATTY_DIRECTION_OUT;

  if (direction == CHATTY_DIRECTION_OUT && uuid) {
    JsonObject *data_unsigned;
    const char *transaction_id;
    guint n_items = 0, limit;

    if (root)
      data_unsigned = matrix_utils_json_object_get_object (root, "unsigned");
    else
      data_unsigned = matrix_utils_json_object_get_object (object, "unsigned");
    transaction_id = matrix_utils_json_object_get_string (data_unsigned, "transaction_id");

    if (transaction_id)
      n_items = g_list_model_get_n_items (G_LIST_MODEL (self->message_list));

    if (n_items > 50)
      limit = n_items - 50;
    else
      limit = 0;

    /* Note: i, limit and n_items are unsigned */
    for (guint i = n_items - 1; i + 1 > limit; i--) {
      g_autoptr(ChattyMessage) msg = NULL;
      const char *event_id;

      msg = g_list_model_get_item (G_LIST_MODEL (self->message_list), i);
      event_id = g_object_get_data (G_OBJECT (msg), "event-id");

      if (event_id && g_str_equal (event_id, transaction_id)) {
        chatty_message_set_uid (msg, uuid);
        chatty_history_add_message (self->history_db, CHATTY_CHAT (self), msg);
        return;
      }
    }
  }

  /* We should move to more precise time (ie, time in ms) as it is already provided */
  message = chatty_message_new (CHATTY_ITEM (buddy), NULL, body, uuid, ts, msg_type, direction, 0);
  chatty_message_set_encrypted (message, encrypted);

  if (msg_type != CHATTY_MESSAGE_TEXT)
    chat_handle_m_media (self, message, content, type, encrypted);

  g_list_store_append (self->message_list, message);
  chatty_history_add_message (self->history_db, CHATTY_CHAT (self), message);
}

static void
handle_m_room_encrypted (ChattyMaChat  *self,
                         ChattyMaBuddy *buddy,
                         JsonObject    *root)
{
  g_autofree char *plaintext = NULL;
  JsonObject *content;

  g_assert (CHATTY_IS_MA_CHAT (self));

  if (!root)
    return;

  content = matrix_utils_json_object_get_object (root, "content");
  if (content)
    plaintext = matrix_enc_handle_join_room_encrypted (self->matrix_enc,
                                                       self->room_id,
                                                       content);

  if (plaintext) {
    g_autoptr(JsonObject) message = NULL;

    message = matrix_utils_string_to_json_object (plaintext);
    matrix_add_message_from_data (self, buddy, root, message, TRUE);
  }
}

static void
matrix_send_message_from_queue (ChattyMaChat *self)
{
  g_assert (CHATTY_IS_MA_CHAT (self));

  while (self->message_queue->length) {
    g_autoptr(ChattyMessage) message = NULL;

    /* TODO: The server may reject if messages are send
     * too soon, so send messages after some delay.
     */
    message = g_queue_pop_head (self->message_queue);
    matrix_api_send_message (self->matrix_api, CHATTY_CHAT (self), self->room_id, message);
  }
}

static void
ma_chat_handle_ephemeral (ChattyMaChat *self,
                          JsonObject   *root)
{
  JsonObject *object;
  JsonArray *array;

  g_assert (CHATTY_IS_MA_CHAT (self));
  g_assert (root);

  array = matrix_utils_json_object_get_array (root, "events");

  if (array) {
    g_autoptr(GList) elements = NULL;

    elements = json_array_get_elements (array);

    for (GList *node = elements; node; node = node->next) {
      const char *type;

      object = json_node_get_object (node->data);
      type = matrix_utils_json_object_get_string (object, "type");
      object = matrix_utils_json_object_get_object (object, "content");

      if (g_strcmp0 (type, "m.typing") == 0) {
        array = matrix_utils_json_object_get_array (object, "user_ids");

        if (array) {
          const char *username, *name = NULL;
          guint typing_count = 0;
          gboolean buddy_typing = FALSE;
          gboolean self_typing = FALSE;

          typing_count = json_array_get_length (array);
          buddy_typing = typing_count >= 2;

          /* Handle the first item so that we don’t have to
             handle buddy_typing in the loop */
          username = matrix_api_get_username (self->matrix_api);
          if (typing_count)
            name = json_array_get_string_element (array, 0);

          if (g_strcmp0 (name, username) == 0)
            self_typing = TRUE;
          else if (typing_count)
            buddy_typing = TRUE;

          /* Check if the server says we are typing too */
          for (guint i = 0; !self_typing && i < typing_count; i++)
            if (g_str_equal (json_array_get_string_element (array, i), username))
              self_typing = TRUE;

          if (self->self_typing != self_typing) {
            self->self_typing = self_typing;
            self->self_typing_set_time = g_get_monotonic_time ();
          }

          if (self->buddy_typing != buddy_typing) {
            self->buddy_typing = buddy_typing;
            g_object_notify (G_OBJECT (self), "buddy-typing");
          }
        }
      }
    }
  }
}

static void
upload_out_group_key_cb (GObject      *obj,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  ChattyMaChat *self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (CHATTY_IS_MA_CHAT (self));

  self->claiming_keys = FALSE;
  if (matrix_api_upload_group_keys_finish (self->matrix_api, result, &error))
    self->keys_claimed = TRUE;

  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("error uploading group keys: %s", error->message);
    return;
  }

  matrix_send_message_from_queue (self);
}

static void
claim_key_cb (GObject      *obj,
              GAsyncResult *result,
              gpointer      user_data)
{
  ChattyMaChat *self = user_data;
  g_autoptr(JsonObject) root = NULL;
  g_autoptr(GList) members = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *room_key = NULL;
  JsonObject *object;

  g_assert (CHATTY_IS_MA_CHAT (self));

  root = matrix_api_claim_keys_finish (self->matrix_api, result, &error);

  if (error) {
    self->claiming_keys = FALSE;
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("error: %s", error->message);
    return;
  }

  object = matrix_utils_json_object_get_object (root, "one_time_keys");
  if (object)
    members = json_object_get_members (object);

  for (GList *member = members; member; member = member->next) {
    ChattyMaBuddy *buddy;
    JsonObject *keys;

    buddy = ma_chat_find_buddy (self, G_LIST_MODEL (self->buddy_list),
                                member->data, NULL);

    if (!buddy) {
      g_warning ("‘%s’ not found in buddy list", (char *)member->data);
      continue;
    }

    keys = matrix_utils_json_object_get_object (object, member->data);
    chatty_ma_buddy_add_one_time_keys (buddy, keys);
  }

  matrix_api_upload_group_keys_async (self->matrix_api,
                                      self->room_id,
                                      G_LIST_MODEL (self->buddy_list),
                                      upload_out_group_key_cb,
                                      self);
}

static void
query_key_cb (GObject      *obj,
              GAsyncResult *result,
              gpointer      user_data)
{
  ChattyMaChat *self = user_data;
  g_autoptr(JsonObject) root = NULL;
  g_autoptr(GList) members = NULL;
  g_autoptr(GError) error = NULL;
  JsonObject *object;

  g_assert (CHATTY_IS_MA_CHAT (self));
  g_return_if_fail (!self->keys_claimed);

  root = matrix_api_query_keys_finish (self->matrix_api, result, &error);

  if (error) {
    self->claiming_keys = FALSE;
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("error: %s", error->message);
    return;
  }

  object = matrix_utils_json_object_get_object (root, "device_keys");
  if (object)
    members = json_object_get_members (object);

  /* TODO: avoid blocked devices (once we implement blocking) */
  for (GList *member = members; member; member = member->next) {
    ChattyMaBuddy *buddy;
    JsonObject *device;

    buddy = ma_chat_find_buddy (self, G_LIST_MODEL (self->buddy_list),
                                member->data, NULL);

    if (!buddy) {
      g_warning ("‘%s’ not found in buddy list", (char *)member->data);
      continue;
    }

    device = matrix_utils_json_object_get_object (object, member->data);
    chatty_ma_buddy_add_devices (buddy, device);
  }

  matrix_api_claim_keys_async (self->matrix_api,
                               G_LIST_MODEL (self->buddy_list),
                               claim_key_cb, self);
}

static void
get_room_state_cb (GObject      *obj,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  ChattyMaChat *self = user_data;
  g_autoptr(GError) error = NULL;
  JsonArray *array;

  g_assert (CHATTY_IS_MA_CHAT (self));

  array = matrix_api_get_room_state_finish (self->matrix_api, result, &error);
  self->state_is_syncing = FALSE;

  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("error: %s", error->message);
    return;
  }

  if (json_array_get_length (array) == 0)
    return;

  self->state_is_sync = TRUE;
  for (guint i = 0; i < json_array_get_length (array); i++) {
    JsonObject *object;
    const char *type;

    object = json_array_get_object_element (array, i);
    type = matrix_utils_json_object_get_string (object, "type");

    if (!type || !*type)
      continue;

    if (g_str_equal (type, "m.room.member"))
      handle_m_room_member (self, object, self->buddy_list);
    else if (g_str_equal (type, "m.room.name"))
      handle_m_room_name (self, object);
    else if (g_str_equal (type, "m.room.encryption"))
      handle_m_room_encryption (self, object);
    /* TODO */
    /* else if (g_str_equal (type, "m.room.power_levels")) */
    /*   handle_m_room_power_levels (self, object); */
    /* else if (g_str_equal (type, "m.room.guest_access")) */
    /*   handle_m_room_guest_access (self, object); */
    /* else if (g_str_equal (type, "m.room.create")) */
    /*   handle_m_room_create (self, object); */
    /* else if (g_str_equal (type, "m.room.history_visibility")) */
    /*   handle_m_room_history_visibility (self, object); */
    /* else if (g_str_equal (type, "m.room.join_rules")) */
    /*   handle_m_room_join_rules (self, object); */
    /* else */
    /*   g_warn_if_reached (); */
  }

  /* Clear pointer so that it’s generated when requested */
  g_clear_pointer (&self->generated_name, g_free);
  g_object_notify (G_OBJECT (self), "name");

  if (self->message_queue->length > 0) {
    if (!self->claiming_keys)
      matrix_api_query_keys_async (self->matrix_api,
                                   G_LIST_MODEL (self->buddy_list),
                                   NULL, query_key_cb, self);
    else if (self->keys_claimed)
      matrix_send_message_from_queue (self);
  }

  g_object_notify (G_OBJECT (self), "name");
  g_signal_emit_by_name (self, "avatar-changed");
}

static void
parse_chat_array (ChattyMaChat *self,
                  JsonArray    *array)
{
  g_autoptr(GList) events = NULL;

  if (!array)
    return;

  events = json_array_get_elements (array);

  for (GList *event = events; event; event = event->next) {
    ChattyMaBuddy *buddy;
    JsonObject *object;
    const char *type, *sender;

    object = json_node_get_object (event->data);
    type = matrix_utils_json_object_get_string (object, "type");
    sender = matrix_utils_json_object_get_string (object, "sender");

    if (!type || !*type || !sender || !*sender)
      continue;

    buddy = ma_chat_find_buddy (self, G_LIST_MODEL (self->buddy_list), sender, NULL);

    if (!buddy)
      buddy = ma_chat_add_buddy (self, self->buddy_list, sender);

    if (!self->self_buddy &&
        g_strcmp0 (sender, chatty_chat_get_username (CHATTY_CHAT (self))) == 0)
      g_set_object (&self->self_buddy, buddy);

    if (g_str_equal (type, "m.room.name")) {
      handle_m_room_name (self, object);
    } else if (g_str_equal (type, "m.room.message")) {
      matrix_add_message_from_data (self, buddy, NULL, object, FALSE);
    } else if (g_str_equal (type, "m.room.encryption")) {
      handle_m_room_encryption (self, object);
    } else if (g_str_equal (type, "m.room.encrypted")) {
      handle_m_room_encrypted (self, buddy, object);
    }
  }

  if (!self->room_name_loaded) {
    self->room_name_loaded = TRUE;
    g_object_notify (G_OBJECT (self), "name");
    g_signal_emit_by_name (self, "avatar-changed");
  }
}

static void
matrix_chat_set_json_data (ChattyMaChat *self,
                           JsonObject   *object)
{
  g_assert (CHATTY_IS_MA_CHAT (self));

  g_clear_pointer (&self->json_data, json_object_unref);
  self->json_data = object;

  if (!object)
    return;

  if (!self->state_is_sync && !self->state_is_syncing) {
    self->state_is_syncing = TRUE;
    matrix_api_get_room_state_async (self->matrix_api,
                                     self->room_id,
                                     get_room_state_cb,
                                     self);
  }

  object = matrix_utils_json_object_get_object (self->json_data, "ephemeral");
  if (object)
    ma_chat_handle_ephemeral (self, object);

  object = matrix_utils_json_object_get_object (self->json_data, "unread_notifications");
  if (object)
    self->highlight_count = matrix_utils_json_object_get_int (object, "highlight_count");

  object = matrix_utils_json_object_get_object (self->json_data, "timeline");
  parse_chat_array (self, matrix_utils_json_object_get_array (object, "events"));

  if (object && matrix_utils_json_object_get_bool (object, "limited"))
    chatty_ma_chat_set_prev_batch (self, g_strdup (matrix_utils_json_object_get_string (object, "prev_batch")));

  object = matrix_utils_json_object_get_object (self->json_data, "unread_notifications");
  if (object) {
    self->unread_count = matrix_utils_json_object_get_int (object, "notification_count");
    g_signal_emit_by_name (self, "changed", 0);
  }
}

static void
get_messages_cb (GObject      *obj,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  ChattyMaChat *self = user_data;
  g_autoptr(JsonObject) root = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (CHATTY_IS_MA_CHAT (self));

  root = matrix_api_load_prev_batch_finish (self->matrix_api, result, &error);
  self->prev_batch_loading = FALSE;

  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("error: %s", error->message);
    return;
  }

  parse_chat_array (self, matrix_utils_json_object_get_array (root, "chunk"));

  /* If start and end are same, we no longer have events to load */
  if (g_strcmp0 (matrix_utils_json_object_get_string (root, "end"),
                 matrix_utils_json_object_get_string (root, "start")) == 0)
    chatty_ma_chat_set_prev_batch (self, NULL);
  else
    chatty_ma_chat_set_prev_batch (self, g_strdup (matrix_utils_json_object_get_string (root, "end")));
}

static gboolean
chatty_ma_chat_is_im (ChattyChat *chat)
{
  return TRUE;
}

static const char *
chatty_ma_chat_get_chat_name (ChattyChat *chat)
{
  return chatty_item_get_name (CHATTY_ITEM (chat));
}

static const char *
chatty_ma_chat_get_username (ChattyChat *chat)
{
  ChattyMaChat *self = (ChattyMaChat *)chat;

  g_assert (CHATTY_IS_MA_CHAT (self));

  return matrix_api_get_username (self->matrix_api);
}

static GListModel *
chatty_ma_chat_get_messages (ChattyChat *chat)
{
  ChattyMaChat *self = (ChattyMaChat *)chat;

  g_assert (CHATTY_IS_MA_CHAT (self));

  return G_LIST_MODEL (self->sorted_message_list);
}

static ChattyAccount *
chatty_ma_chat_get_account (ChattyChat *chat)
{
  ChattyMaChat *self = (ChattyMaChat *)chat;

  g_assert (CHATTY_IS_MA_CHAT (self));

  return self->account;
}


static ChattyEncryption
chatty_ma_chat_get_encryption (ChattyChat *chat)
{
  ChattyMaChat *self = (ChattyMaChat *)chat;

  g_assert (CHATTY_IS_MA_CHAT (self));

  if (self->encryption)
    return CHATTY_ENCRYPTION_ENABLED;

  return CHATTY_ENCRYPTION_DISABLED;
}

static const char *
chatty_ma_chat_get_last_message (ChattyChat *chat)
{
  ChattyMaChat *self = (ChattyMaChat *)chat;
  g_autoptr(ChattyMessage) message = NULL;
  GListModel *model;
  guint n_items;

  g_assert (CHATTY_IS_MA_CHAT (self));

  model = G_LIST_MODEL (self->message_list);
  n_items = g_list_model_get_n_items (model);

  if (n_items == 0)
    return "";

  message = g_list_model_get_item (model, n_items - 1);

  return chatty_message_get_text (message);
}

static guint
chatty_ma_chat_get_unread_count (ChattyChat *chat)
{
  ChattyMaChat *self = (ChattyMaChat *)chat;

  g_assert (CHATTY_IS_MA_CHAT (self));

  return self->unread_count;
}

static void
chat_set_read_marker_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  ChattyMaChat *self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (CHATTY_IS_MA_CHAT (self));

  if (matrix_api_set_read_marker_finish (self->matrix_api, result, &error)) {
    self->unread_count = 0;
    g_signal_emit_by_name (self, "changed", 0);
  } else if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("Error updating read marker: %s", error->message);
}

static void
chatty_ma_chat_set_unread_count (ChattyChat *chat,
                                 guint       unread_count)
{
  ChattyMaChat *self = (ChattyMaChat *)chat;

  g_assert (CHATTY_IS_MA_CHAT (self));

  if (self->unread_count == unread_count)
    return;

  if (unread_count == 0) {
    g_autoptr(ChattyMessage) message = NULL;
    GListModel *model;
    guint n_items;

    model = G_LIST_MODEL (self->message_list);
    n_items = g_list_model_get_n_items (model);

    if (n_items == 0)
      return;

    message = g_list_model_get_item (model, n_items - 1);
    matrix_api_set_read_marker_async (self->matrix_api, self->room_id, message,
                                      chat_set_read_marker_cb, self);
  } else {
    self->unread_count = unread_count;
    g_signal_emit_by_name (self, "changed", 0);
  }
}

static gboolean
chatty_ma_chat_get_buddy_typing (ChattyChat *chat)
{
  ChattyMaChat *self = (ChattyMaChat *)chat;

  g_assert (CHATTY_IS_MA_CHAT (self));

  return self->buddy_typing;
}

static const char *
chatty_ma_chat_get_name (ChattyItem *item)
{
  ChattyMaChat *self = (ChattyMaChat *)item;

  g_assert (CHATTY_IS_MA_CHAT (self));

  if (self->room_name)
    return self->room_name;

  if (!self->room_name_loaded)
    return self->room_id;

  if (!self->generated_name)
    chatty_mat_chat_update_name (self);

  if (self->generated_name)
    return self->generated_name;

  if (self->room_id)
    return self->room_id;

  return "";
}


static ChattyProtocol
chatty_ma_chat_get_protocols (ChattyItem *item)
{
  return CHATTY_PROTOCOL_MATRIX;
}

static void
chatty_ma_chat_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  ChattyMaChat *self = (ChattyMaChat *)object;

  switch (prop_id)
    {
    case PROP_JSON_DATA:
      matrix_chat_set_json_data (self, g_value_dup_boxed (value));
      break;

    case PROP_ROOM_ID:
      self->room_id = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
chatty_ma_chat_finalize (GObject *object)
{
  ChattyMaChat *self = (ChattyMaChat *)object;

  g_list_store_remove_all (self->message_list);
  g_clear_object (&self->message_list);
  g_clear_object (&self->matrix_api);
  g_clear_object (&self->matrix_enc);
  g_queue_free_full (self->message_queue, g_object_unref);

  g_free (self->room_name);
  g_free (self->generated_name);
  g_free (self->room_id);
  g_free (self->encryption);
  g_free (self->prev_batch);
  g_free (self->last_batch);

  G_OBJECT_CLASS (chatty_ma_chat_parent_class)->finalize (object);
}

static void
chatty_ma_chat_class_init (ChattyMaChatClass *klass)
{
  GObjectClass *object_class  = G_OBJECT_CLASS (klass);
  ChattyItemClass *item_class = CHATTY_ITEM_CLASS (klass);
  ChattyChatClass *chat_class = CHATTY_CHAT_CLASS (klass);

  object_class->set_property = chatty_ma_chat_set_property;
  object_class->finalize = chatty_ma_chat_finalize;

  item_class->get_name = chatty_ma_chat_get_name;
  item_class->get_protocols = chatty_ma_chat_get_protocols;

  chat_class->is_im = chatty_ma_chat_is_im;
  chat_class->get_chat_name = chatty_ma_chat_get_chat_name;
  chat_class->get_username = chatty_ma_chat_get_username;
  chat_class->get_messages = chatty_ma_chat_get_messages;
  chat_class->get_account  = chatty_ma_chat_get_account;
  chat_class->get_encryption = chatty_ma_chat_get_encryption;
  chat_class->get_last_message = chatty_ma_chat_get_last_message;
  chat_class->get_unread_count = chatty_ma_chat_get_unread_count;
  chat_class->set_unread_count = chatty_ma_chat_set_unread_count;
  chat_class->get_buddy_typing = chatty_ma_chat_get_buddy_typing;

  properties[PROP_JSON_DATA] =
    g_param_spec_boxed ("json-data",
                        "json-data",
                        "json-data for the room",
                        JSON_TYPE_OBJECT,
                        G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ROOM_ID] =
    g_param_spec_string ("room-id",
                         "json-data",
                         "json-data for the room",
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
chatty_ma_chat_init (ChattyMaChat *self)
{
  g_autoptr(GtkSorter) sorter = NULL;

  sorter = gtk_custom_sorter_new (sort_message, NULL, NULL);

  self->message_list = g_list_store_new (CHATTY_TYPE_MESSAGE);
  self->sorted_message_list = gtk_sort_list_model_new (G_LIST_MODEL (self->message_list), sorter);
  self->buddy_list = g_list_store_new (CHATTY_TYPE_MA_BUDDY);
  self->message_queue = g_queue_new ();
}

void
chatty_ma_chat_set_history_db (ChattyMaChat *self,
                               gpointer      history_db)
{
  g_return_if_fail (CHATTY_IS_MA_CHAT (self));
  g_return_if_fail (CHATTY_IS_HISTORY (history_db));
  g_return_if_fail (!self->history_db);

  self->history_db = g_object_ref (history_db);
}

void
chatty_ma_chat_set_matrix_db (ChattyMaChat *self,
                              gpointer      matrix_db)
{
  g_return_if_fail (CHATTY_IS_MA_CHAT (self));
  g_return_if_fail (MATRIX_IS_DB (matrix_db));
  g_return_if_fail (!self->matrix_db);

  self->matrix_db = g_object_ref (matrix_db);
}

/**
 * chatty_ma_chat_set_data:
 * @self: a #ChattyMaChat
 * @account: A #ChattyMaAccount
 * @api: A #MatrixApi
 * @enc: A #MatrixEnc for E2E
 *
 * Use this function to set internal data required
 * to connect to a matrix server.
 */
void
chatty_ma_chat_set_data (ChattyMaChat  *self,
                         ChattyAccount *account,
                         gpointer       api,
                         gpointer       enc)
{
  g_return_if_fail (CHATTY_IS_MA_CHAT (self));
  g_return_if_fail (MATRIX_IS_API (api));
  g_return_if_fail (MATRIX_IS_ENC (enc));

  g_set_object (&self->account, account);
  g_set_object (&self->matrix_api, api);
  g_set_object (&self->matrix_enc, enc);
}

gboolean
chatty_ma_chat_matches_id (ChattyMaChat *self,
                           const char   *room_id)
{
  g_return_val_if_fail (CHATTY_IS_MA_CHAT (self), FALSE);

  if (!self->room_id)
    return FALSE;

  return g_strcmp0 (self->room_id, room_id) == 0;
}

const char *
chatty_ma_chat_get_room_id (ChattyMaChat *self)
{
  g_return_val_if_fail (CHATTY_IS_MA_CHAT (self), NULL);

  return self->room_id;
}

void
chatty_ma_chat_set_typing (ChattyMaChat *self,
                           gboolean      is_typing)
{
  g_return_if_fail (CHATTY_IS_MA_CHAT (self));

  /* Send typing request atmost once every 5 seconds */
  if (self->self_typing == !!is_typing &&
      (g_get_monotonic_time () - self->self_typing_set_time) < G_TIME_SPAN_SECOND * 5)
    return;

  self->self_typing = !!is_typing;
  self->self_typing_set_time = g_get_monotonic_time ();
  matrix_api_set_typing (self->matrix_api, self->room_id, !!is_typing);
}

void
chatty_ma_chat_send_message (ChattyMaChat *self,
                             const char   *msg)
{
  ChattyChat *chat = (ChattyChat *)self;
  g_autoptr(ChattyMessage) message = NULL;

  g_return_if_fail (CHATTY_IS_MA_CHAT (self));
  g_return_if_fail (msg && *msg);

  message = chatty_message_new (CHATTY_ITEM (self->self_buddy), NULL,
                                msg, NULL, time (NULL), CHATTY_MESSAGE_TEXT,
                                CHATTY_DIRECTION_OUT, CHATTY_STATUS_SENDING);
  g_list_store_append (self->message_list, message);
  if (chatty_chat_get_encryption (chat) != CHATTY_ENCRYPTION_ENABLED ||
      self->keys_claimed) {
    matrix_api_send_message (self->matrix_api, chat, self->room_id, message);
  } else {
    g_queue_push_tail (self->message_queue, g_object_ref (message));
    if (!self->state_is_syncing && !self->claiming_keys)
      matrix_api_query_keys_async (self->matrix_api,
                                   G_LIST_MODEL (self->buddy_list),
                                   NULL, query_key_cb, self);
  }
}

void
chatty_ma_chat_set_prev_batch (ChattyMaChat *self,
                               char         *prev_batch)
{
  g_return_if_fail (CHATTY_IS_MA_CHAT (self));

  g_clear_pointer (&self->prev_batch, g_free);
  self->prev_batch = prev_batch;
}

/**
 * chatty_ma_chat_set_last_batch:
 * @self: A #ChattyMaChat
 * @last_batch: A string representing time
 *
 * The batch which we have already loaded chat
 * up to. When history is loaded from server,
 * only items after @last_batch shall be loaded.
 * This limits only loading history from server
 * not from database.
 */
void
chatty_ma_chat_set_last_batch (ChattyMaChat *self,
                               const char   *last_batch)
{
  g_return_if_fail (CHATTY_IS_MA_CHAT (self));

  g_free (self->last_batch);
  self->last_batch = g_strdup (last_batch);
}

void
chatty_ma_chat_load_past_messages (ChattyMaChat *self)
{
  g_return_if_fail (CHATTY_IS_MA_CHAT (self));

  if (!self->prev_batch || self->prev_batch_loading)
    return;

  self->prev_batch_loading = TRUE;
  matrix_api_load_prev_batch_async (self->matrix_api,
                                    self->room_id,
                                    self->prev_batch,
                                    self->last_batch,
                                    get_messages_cb, self);
}

void
chatty_ma_chat_add_messages (ChattyMaChat *self,
                             GPtrArray    *messages)
{
  g_return_if_fail (CHATTY_IS_MA_CHAT (self));

  if (messages && messages->len)
    g_list_store_splice (self->message_list, 0, 0,
                         messages->pdata, messages->len);
}