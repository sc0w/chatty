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
#include "dialogs/chatty-dialogs.h"
#include "chatty-folks.h"


static void
chatty_connection_update_ui (void)
{
  ChattyWindow *window;
  GList        *accounts;
  gboolean      sensitive;

  window = chatty_utils_get_window ();

  chatty_window_set_im_account_connected (window, FALSE);
  chatty_window_set_sms_account_connected (window, FALSE);

  chatty_window_set_button_group_chat_sensitive (window, FALSE);

  for (accounts = purple_accounts_get_all (); accounts != NULL; accounts = accounts->next) {
    ChattyPpAccount *account;

    account = chatty_pp_account_get_object (accounts->data);

    /* The account should exist */
    if (!CHATTY_IS_PP_ACCOUNT (account))
      {
        g_warn_if_reached ();
        continue;
      }

    if (chatty_account_get_status (CHATTY_ACCOUNT (account)) == CHATTY_CONNECTED) {
      if (chatty_pp_account_is_sms (account)) {
        chatty_window_set_sms_account_connected (window, TRUE);
      }  else {
        chatty_window_set_im_account_connected (window, TRUE);
        chatty_window_set_button_group_chat_sensitive (window, TRUE);
      } 
    }
  }

  sensitive = chatty_window_get_sms_account_connected (window) |
              chatty_window_get_im_account_connected (window);

  chatty_window_set_header_add_chat_button_sensitive (window, sensitive);
  chatty_window_update_overlay_visible (window);
}


static void
chatty_connection_error_dialog (ChattyPpAccount *account,
                                const gchar     *error)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (NULL,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_OK,
                                   _("Login failed"));

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG(dialog),
                                            "%s: %s\n\n%s",
                                            error,
                                            chatty_pp_account_get_username (account),
                                            _("Please check ID and password"));

  gtk_dialog_set_default_response (GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
  gtk_window_set_position (GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

  gtk_dialog_run (GTK_DIALOG(dialog));

  gtk_widget_destroy (dialog);
}


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

  chatty_connection_update_ui ();

  g_debug ("%s account: %s", __func__, chatty_pp_account_get_username (pp_account));
}


static void
chatty_connection_disconnected (PurpleConnection *gc)
{
  chatty_connection_update_ui ();

  g_debug ("chatty_connection_disconnected");
}


static void
chatty_connection_report_disconnect_reason (PurpleConnection     *gc,
                                            PurpleConnectionError reason,
                                            const char           *text)
{
  ChattyPpAccount  *pp_account;
  PurpleAccount    *account;
  g_autofree gchar *message = NULL;
  const char       *user_name;
  const char       *protocol_id;

  account = purple_connection_get_account (gc);
  pp_account = chatty_pp_account_get_object (account);
  g_return_if_fail (CHATTY_IS_PP_ACCOUNT (pp_account));

  user_name = chatty_pp_account_get_username (pp_account);
  protocol_id = chatty_pp_account_get_protocol_id (pp_account);

  g_debug ("Disconnected: \"%s\" (%s)\nError: %d\nReason: %s",
           user_name, protocol_id, reason, text);

  if (purple_connection_error_is_fatal (reason))
    chatty_connection_error_dialog (pp_account, text);
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
  chatty_connection_disconnected,
  chatty_connection_info,
  NULL,
  NULL,
  NULL,
  chatty_connection_report_disconnect_reason,
  NULL,
  NULL,
  NULL
};


PurpleConnectionUiOps *
chatty_connection_get_ui_ops (void)
{
  return &connection_ui_ops;
}
