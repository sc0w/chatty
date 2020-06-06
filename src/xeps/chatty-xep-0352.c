/*
 * Copyright (C) 2020 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Guido Günther <agx@sigxcpu.org>
 */

#define G_LOG_DOMAIN "chatty-xep-0352"

#include "users/chatty-pp-account.h"
#include "chatty-xep-0352.h"
#include "xeps.h"

#include <purple.h>
#include "prpl.h"
#include "xmlnode.h"
#include "jabber.h"

#include <glib.h>


/*
 * Client state indication as per xep-0352
 */

static PurpleCmdId csi_cmd_handle_id;
static guint handle;

static void
csi_set_active (PurpleConnection *pc, gboolean active)
{
  g_autoptr(xmlnode) state = NULL;

  /* PurpleConnection is not a GObject in 2.x */
  g_return_if_fail (pc);

  state = xmlnode_new(active ? "active" : "inactive");
  xmlnode_set_namespace(state, CHATTY_XEPS_NS_CSI);
  purple_signal_emit(purple_connection_get_prpl(pc), "jabber-sending-xmlnode", pc, &state);
}

static void
on_xmlnode_received (PurpleConnection  *gc,
		     xmlnode          **packet,
		     gpointer           null)
{
  const char *name;
  const char *xmlns;

  g_return_if_fail (*packet);

  name = (*packet)->name;
  xmlns = xmlnode_get_namespace(*packet);

  if (!g_strcmp0 (xmlns, CHATTY_XEPS_NS_STREAM)
      && !g_strcmp0 (name, "features")) {
    g_autoptr(xmlnode) csi_node = xmlnode_get_child (*packet, "csi");
    PurpleAccount *pa = purple_connection_get_account (gc);

    g_return_if_fail (pa);

    if (csi_node) {
      xmlns = xmlnode_get_namespace (csi_node);
      if (!g_strcmp0 (xmlns, CHATTY_XEPS_NS_CSI)) {
	ChattyPpAccount *ca = chatty_pp_account_get_object (pa);
	g_return_if_fail (ca);
	g_debug ("Server of %s supports CSI", purple_account_get_username (pa));
	chatty_pp_account_update_features (ca, CHATTY_PP_ACCOUNT_FEATURES_CSI);
      }
    }
  }
}

static PurpleCmdRet
csi_cmd_func (PurpleConversation *conv,
	      const gchar        *cmd,
	      gchar             **args,
	      gchar             **error,
	      void               *unused)
{
  g_autofree gchar *msg = NULL;
  PurpleConnection *conn = purple_conversation_get_gc (conv);

  if (!g_strcmp0 (args[0], "help")) {
    msg = g_strdup("Client state indication:\n\n"
		   " - '/csi active': Set state to active.\n"
		   " - '/csi inactive': Set state to inactive.\n"
		   " - '/csi caps': Check server capabilities.\n"
		   " - '/csi help': Show help.\n"
		   "\n");
  } else if (!g_strcmp0 (args[0], "active")) {
    csi_set_active (conn, TRUE);
  } else if (!g_strcmp0 (args[0], "inactive")) {
    csi_set_active (conn, FALSE);
  } else if (!g_strcmp0 (args[0], "caps")) {
    PurpleAccount *pa = purple_connection_get_account (conn);
    ChattyPpAccount *ca = chatty_pp_account_get_object (pa);

    msg = g_strdup_printf ("Server of %s does%s support client "
			   "state indication", purple_account_get_username (pa),
			   chatty_pp_account_has_features (ca, CHATTY_PP_ACCOUNT_FEATURES_CSI) ? "" : " not");
  } else {
    msg = g_strdup ("Unknown command, try /csi help");
  }

  if (msg)
    purple_conversation_write (conv, "csi", msg, PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG,
			       time (NULL));

  return PURPLE_CMD_RET_OK;
}

void
chatty_0352_close (void)
{
  purple_cmd_unregister (csi_cmd_handle_id);
  purple_signals_disconnect_by_handle ((gpointer)&handle);
}

void
chatty_0352_init (void)
{
  g_return_if_fail (csi_cmd_handle_id == 0);

  csi_cmd_handle_id = purple_cmd_register("csi",
					  "w",
					  PURPLE_CMD_P_PLUGIN,
					  PURPLE_CMD_FLAG_IM
					  | PURPLE_CMD_FLAG_CHAT
					  | PURPLE_CMD_FLAG_PRPL_ONLY
					  | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS,
					  CHATTY_XEPS_JABBER_PROTOCOL_ID,
					  csi_cmd_func,
					  "csi &lt;help&gt;:  "
					  "Interface to client state indication. For details, use the 'help' argument.",
					  NULL);

  purple_signal_connect (chatty_xeps_get_jabber (),
                         "jabber-receiving-xmlnode",
                         &handle,
                         PURPLE_CALLBACK(on_xmlnode_received),
                         NULL);
}
