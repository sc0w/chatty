/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* account.c
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

#include "purple-init.h"
#include "chatty-pp-account.h"

static void
test_new_account (void)
{
  ChattyPpAccount *account = NULL;
  PurpleAccount *pp_account;
  const gchar *str;

  account = chatty_pp_account_new ("XMPP", "prpl-jabber");
  g_assert (CHATTY_IS_PP_ACCOUNT (account));

  str = chatty_pp_account_get_protocol_id (account);
  g_assert_cmpstr (str, ==, "prpl-jabber");

  str = chatty_pp_account_get_protocol_name (account);
  g_assert_cmpstr (str, ==, "XMPP");

  pp_account = chatty_pp_account_get_account (account);
  g_assert_nonnull (pp_account);

  g_object_unref (account);

  account = chatty_pp_account_new_purple (pp_account);
  g_assert (CHATTY_IS_PP_ACCOUNT (account));

  str = chatty_pp_account_get_protocol_id (account);
  g_assert_cmpstr (str, ==, "prpl-jabber");

  g_object_unref (account);
}

int
main (int   argc,
      char *argv[])
{
  int ret;

  g_test_init (&argc, &argv, NULL);

  test_purple_init ();

  g_test_add_func ("/account/new", test_new_account);

  ret = g_test_run ();

  /* FIXME: purple_core_quit() results in more leak! */
  purple_core_quit ();

  return ret;
}
