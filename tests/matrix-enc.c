/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* matrix-utils.c
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef NDEBUG
#undef G_DISABLE_ASSERT
#undef G_DISABLE_CHECKS
#undef G_DISABLE_CAST_CHECKS
#undef G_LOG_DOMAIN

#include <glib.h>
#include <olm/olm.h>
#include <sys/random.h>

#include "matrix/matrix-utils.h"
#include "matrix/matrix-enc.h"

typedef struct EncData {
  MatrixEnc *enc;
  char *user_id;
  char *device_id;
  char *pickle_key;
  char *pickle;
  char *curve_key;
  char *ed_key;
  /* These are not marked as published */
  char *one_time_key1;
  char *one_time_key2;
  char *one_time_key3;
} EncData;

EncData enc[] = {
  {
    NULL,
    "@neo:example.com",
    "JOJOAREBZY",
    "cefdef40-3b16-4d71-8685-2740833c3297",
    "HoxM0prMq/t3pGK6OLSet1h/1H8bnILjY1fp2Colr/ATOFevqulWPpiyktCJ0Cw1KZGQvHzHI481s2n2xu7yJkVC4TxrX5gKNTQ2QevZpVKD4PEmRaq40twVWOBjuAoxch16LCZs7CjNCYGtR7vhO2M6s5YchJhMFXJmH0Seik+yP5vyPJDx7nLcS6PZWEYGtr5U3VFUf4pj453NxrX7bAy7HRQOY642Vqkd1F06w6367naS0/0cgMj1aWDSK+z+a+F7TNfOaKky+90hLTYtkyM8VbFx8e6BmrmTyheCxpGKC1JQnj0QGCKb8cxAgV3FIaol8lOoFngA1Q7m6aEvkh3PyEqCUHqXlt70E0zHZy021tavDG2g1+GaCar9hOPayfVNpPZdFrL+avUJnTQ6DnxiGcBpobv+JiBjdOMb7rpG80P9nkPwre1D+JTvbCKgI+kZHUX3+Xs6YX01JJ8gmArl9GT1k4WkDi3iR74AAynXAASJRuwD6oKUbt/R5Ey3eqcmMh1IrwI",
    "UpcUBGKMHofzYsMwU5yBM79kL8dAMYXuPav+V1hZnkc",
    "w33QBdjIgxJtP8jnFVCdAgjuCXy0XXT5eIPHJwvmbJw",
    "YbFN9bWf0dfQCbsxsx5Lo779zXKAD/ctCN4qEj8G0T8",
    "zNejRv4W41qroEyRmfxmwc2A9fliPsqcpJTb2E5woR0",
    "OeS+WTn5e8k6C1jH4Mw1Otl3of0t+jPvWh2GusmlfBY",
  },
  {
    NULL,
    "@neo:example.org",
    "JDFVAREDFE",
    "bc80e9a7-f175-4352-9b07-7178b63546ff",
    "GeRh9PcLuV4diHtRBPngRcVVz54++xGLCvYuHKD0kMCpKHejUodLlpxCfI3Ax25E3nAqN7/k++BWywscZRyNqxVJ0J/w46RgiBrAm2+K3u1ylpSxREf0DwYGXSlMBEA7En5FdZkpcGcM83CiSs/3IqmTOMfMq66u7Uoe8wQ9ZJ6KiYMpv9lswkS0xypC0LnEQQWY8A57QKg9zqXhKvQPwQ1AhxaxWGxYaLQ5yYJZFwd+on3mpK+j7Q4BlM1fqA6hrTTBJ/UldBd8Algc5O6UIpB2Xp31ZDmLb9/D76P2/WD12I61x2T1V6WgmNneKRCXTN4PkPCbk+06QXAjL6OAKoK9TH17Hdu2uNGc83ye5reK1ZSfiHklU5NeyCI/ttJn/Pa0I6VnfhcgisxL+TgsXbgqHmi7GD6tvRaeVVr3x9QCOPyGFehaLF20TUZDm8EtbjupMbCTFWb5LU07yT4wM/mU/OsHVgJ1gQCCHrdGuZomyS9XZnkVLGQo+kT+VnUOeTmLznSROV0",
    "FMXixHL3B/8xYAOs6MK4EeghAXZppZgNjb8o9UK4NSg",
    "mnfxCgqI0trldd23vrz1SGbqQDW8TXcKKRlo9M3Mj64",
    "d9lE1FD5s9I0gT4kT5jgQYDSDvNKQuBAQM/5LaSVtms",
    "cq0RN7usZJobRbTAl/7f+ynywnCuaefwQeMmEZJHMwA",
    "/a/OrFWpizYc+OgFz4GrGqsFpyHZhRsC2w3vw1I8wTk",
  },
};


static void
test_matrix_enc_verify (void)
{
  JsonObject *object, *root;
  char *sign, *json, *key_label;
  const char *message;
  EncData data;

  data = enc[0];
  g_assert (MATRIX_IS_ENC (data.enc));

  /* @message is in canonical form */
  message = "{\"timeout\":20000,\"type\":\"m.message\"}";
  sign = matrix_enc_sign_string (data.enc, message, -1);
  g_assert_nonnull (sign);

  root = matrix_utils_string_to_json_object (message);
  g_assert_nonnull (root);

  json_object_set_object_member (root, "signatures", json_object_new ());
  object = json_object_get_object_member (root, "signatures");
  json_object_set_object_member (object, data.user_id, json_object_new ());
  object = json_object_get_object_member (object, data.user_id);
  g_assert (object);

  key_label = g_strconcat ("ed25519:", data.device_id, NULL);
  json_object_set_string_member (object, key_label, sign);
  json = matrix_utils_json_object_to_string (root, FALSE);
  g_assert_nonnull (json);
  g_free (sign);
  g_free (key_label);
  json_object_unref (root);

  root = matrix_utils_string_to_json_object (json);
  g_assert_nonnull (root);
  g_assert_true (matrix_enc_verify (data.enc, root, data.user_id,
                                    data.device_id, data.ed_key));
  g_assert_true (matrix_enc_verify (enc[1].enc, root, data.user_id,
                                    data.device_id, data.ed_key));
  g_assert_false (matrix_enc_verify (data.enc, root, data.user_id,
                                    data.device_id, data.curve_key));
  g_assert_false (matrix_enc_verify (data.enc, root, enc[1].user_id,
                                    data.device_id, data.ed_key));
  json_object_unref (root);
  g_free (json);
}

static void
test_matrix_enc_new (void)
{
  const char *value;
  char *pickle;
  EncData data;

  for (guint i = 0; i < G_N_ELEMENTS (enc); i++) {
    data = enc[i];
    g_assert (MATRIX_IS_ENC (data.enc));

    value = matrix_enc_get_curve25519_key (data.enc);
    g_assert_cmpstr (value, ==, data.curve_key);

    value = matrix_enc_get_ed25519_key (data.enc);
    g_assert_cmpstr (value, ==, data.ed_key);

    pickle = matrix_enc_get_account_pickle (data.enc);
    g_assert_cmpstr (pickle, ==, data.pickle);
    g_clear_pointer (&pickle, g_free);
  }
}

int
main (int   argc,
      char *argv[])
{
  EncData *data;

  data = &enc[0];
  data->enc = matrix_enc_new (NULL, data->pickle, data->pickle_key);
  g_assert (MATRIX_IS_ENC (data->enc));
  matrix_enc_set_details (data->enc, data->user_id, data->device_id);

  data = &enc[1];
  data->enc = matrix_enc_new (NULL, data->pickle, data->pickle_key);
  g_assert (MATRIX_IS_ENC (data->enc));
  matrix_enc_set_details (data->enc, data->user_id, data->device_id);

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/matrix/enc/new", test_matrix_enc_new);
  g_test_add_func ("/matrix/enc/verify", test_matrix_enc_verify);

  return g_test_run ();
}
