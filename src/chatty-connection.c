/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-connection"

#include <glib.h>
#include <glib/gi18n.h>
#include "purple.h"
#include "chatty-window.h"
#include "chatty-connection.h"
#include "users/chatty-pp-account.h"
#include "chatty-buddy-list.h"
#include "chatty-purple-init.h"
#include "chatty-utils.h"
#include "chatty-notify.h"
#include "chatty-folks.h"


static void
chatty_connection_connected (PurpleConnection *gc)
{
  ChattyWindow    *window;
  ChattyPpAccount *pp_account;
  PurpleAccount   *account;
  const char      *uri = NULL;

  account = purple_connection_get_account (gc);
  pp_account = chatty_pp_account_get_object (account);
  g_return_if_fail (CHATTY_IS_PP_ACCOUNT (pp_account));

  if (chatty_pp_account_is_sms (pp_account))
    {
      /* chatty_blist_enable_folks_contacts (); */

      window = chatty_utils_get_window ();
      
      uri = chatty_window_get_uri (window);

      // we are ready to open URI links now
      if (uri) {
        chatty_blist_add_buddy_from_uri (uri);
      }
    }

  g_debug ("%s account: %s", __func__, chatty_pp_account_get_username (pp_account));
}


static void
chatty_connection_info (PurpleConnection *gc,
                        const char       *text)
{
  g_debug ("%s: %s", __func__, text);
}


static PurpleConnectionUiOps connection_ui_ops =
{
  NULL,
  chatty_connection_connected,
  NULL,
  chatty_connection_info,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};


PurpleConnectionUiOps *
chatty_connection_get_ui_ops (void)
{
  return &connection_ui_ops;
}
