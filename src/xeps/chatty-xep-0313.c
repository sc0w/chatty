/*
 * Copyright (C) 2018 Purism SPC
 * Copyright (C) 2019 Ruslan N. Marchenko
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-xeps"

#include <glib.h>
#include <prpl.h>
#include <xmlnode.h>
#include <account.h>
#include <version.h>
#include "jabber.h"
#include "message.h"
#include "xeps.h"
#include "chatty-xep-0313.h"
#include "chatty-utils.h"
#include "chatty-history.h"
#include "chatty-message-list.h"
#include "chatty-conversation.h"
#include "chatty-purple-init.h"

#define NS_FWDv0 "urn:xmpp:forward:0"
#define NS_SIDv0 "urn:xmpp:sid:0"
#define NS_MAMv2 "urn:xmpp:mam:2"

typedef struct {
  PurpleConvMessage p;
  char *id;
  PurpleConversationType type;
} MamMsg;
static GHashTable *ht_bare_ctx = NULL;
/* FIXME: What if purple becomes multithreaded 8-O */
static MamMsg *inflight_msg = NULL;

static void
cb_chatty_mam_bare_info (PurpleConnection *pc,
                          const char *bare,
                          const char *var)
{
  if(g_strcmp0(var,NS_MAMv2) == 0) {
    g_debug ("Server supports MAM %s on %s", var, bare);
    // Init CTX
    // Request MAM backlog
  }
}

static void
cb_chatty_mam_msg_wrote(PurpleAccount *pa, PurpleConvMessage *pcm,
                        char **uuid, PurpleConversationType type,
                        void *ctx)
{
  if(inflight_msg == NULL)
    return;

  // Skip non-jabber writes
  if(g_strcmp0 ("prpl-jabber", purple_account_get_protocol_id (pa)))
    return;

  inflight_msg->p.what = g_strdup(pcm->what);
  // If flags are already set - enforce them, otherwise copy
  // Also always set NO_LOG flag to suppress in-app archiving
  if(inflight_msg->p.flags) {
    pcm->flags = inflight_msg->p.flags | PURPLE_MESSAGE_NO_LOG;
  } else {
    inflight_msg->p.flags = pcm->flags;
    pcm->flags |= PURPLE_MESSAGE_NO_LOG;
  }
  inflight_msg->type = type;
  inflight_msg->p.alias = pcm->alias;
  inflight_msg->p.when = pcm->when;
  inflight_msg->p.who = g_strdup(pcm->who);
  if(inflight_msg->id)
    *uuid = g_strdup(inflight_msg->id);
  g_debug ("Received message on %s of type %d with flags %d",
            inflight_msg->p.alias, inflight_msg->type, inflight_msg->p.flags);
}

static gboolean
cb_chatty_mam_msg_receiving(PurpleAccount *pa, char **who, char **msg,
                            PurpleConversation *conv, PurpleMessageFlags *flags)
{
  // This is a good chance to flip the flags early, not sure we need it though
  g_debug ("Receiving msg on %p from %s with flags %d", conv, *who, *flags);
  return FALSE;
}

/**
 * cb_chatty_mam_msg_received:
 * @pc: a PurpleConnection
 * @msg: a message xmlnode
 *
 * This function is called via the "jabber-receiving-message" signal
 * and is intended to intercept the parser (return TRUE)
 *
 */
static gboolean
cb_chatty_mam_msg_received (PurpleConnection *pc,
                            const char *type, const char *id,
                            const char *from, const char *to,
                            xmlnode *msg)
{
  xmlnode    *node_result;
  xmlnode    *node_sid;
  xmlnode    *message;
  const char *peer;
  const char *query_id;
  const char *stanza_id = NULL;
  const char *stamp = NULL;
  const char *user;
  PurpleMessageFlags flags = 0;
  JabberStream  *js = purple_connection_get_protocol_data (pc);

  if (msg == NULL)
    return FALSE;

  node_result = xmlnode_get_child_with_namespace (msg, "result", NS_MAMv2);
  node_sid    = xmlnode_get_child_with_namespace (msg, "stanza-id", NS_SIDv0);

  if(inflight_msg != NULL) {
    // Skip resubmission - break the loop
    g_debug ("Received resubmission %s %s %s", id, from, to);
    return FALSE;
  }

  if(node_result != NULL || node_sid != NULL) {
    if(node_result != NULL) {
      xmlnode    *node_fwd;
      xmlnode    *node_delay;
      int dts;
      query_id = xmlnode_get_attrib (node_result, "queryid");
      stanza_id = xmlnode_get_attrib (node_result, "id");
      user = purple_account_get_username(purple_connection_get_account(pc));

      // TODO: Check result and query-id are valid
      if(from != NULL && g_strcmp0(from, user)==0) {
        // Fake result injection?
        g_debug ("Fake MAM result injection from %s", from);
        return FALSE; // TODO: uncomment after test
      }

      node_fwd = xmlnode_get_child_with_namespace (node_result, "forwarded", NS_FWDv0);
      if(node_fwd == NULL) // this is rather unexpected, yield
        return FALSE;

      message = xmlnode_get_child (node_fwd, "message");
      node_delay = xmlnode_get_child (node_fwd, "delay");
      peer = xmlnode_get_attrib (message, "from");
      if(node_delay != NULL) {
        stamp = xmlnode_get_attrib (node_delay, "stamp");
        /* Copy delay down for the parser */
        xmlnode_insert_child (message, xmlnode_copy (node_delay));
      }
      // check history and drop the dup
      dts = get_im_timestamp_for_uuid(stanza_id, user);
      if(dts<INT_MAX) {
        g_debug ("Message id %s for acc %s is already stored on %d", stanza_id, user, dts);
        return TRUE; // note - true means stop processing
      }
      // Swap from/to for outgoing messages
      if(peer) {
        char *bare_peer = chatty_utils_jabber_id_strip(peer);
        if(g_strcmp0(user, bare_peer) == 0) {
          // FIXME: It could be communication between user's resources
          char *msg_to = g_strdup (xmlnode_get_attrib (message, "to"));
          xmlnode_set_attrib (message, "to", peer);
          xmlnode_set_attrib (message, "from", msg_to);
          g_free (msg_to);
          flags |= PURPLE_MESSAGE_SEND;
        }
        g_free (bare_peer);
      } else {
        xmlnode_set_attrib (message, "from", xmlnode_get_attrib (message, "to"));
        peer = xmlnode_get_attrib (message, "from");
        flags |= PURPLE_MESSAGE_SEND;
      }
      g_debug ("Received result %s for query_id %s dated %s", stanza_id, query_id, stamp);
    } else {
      stanza_id = xmlnode_get_attrib (node_sid, "id");
      // If it's forward notification of the archive-id (SID) - we need to
      // store the SID in history at the least - to know where to start.
      message = msg;
      peer = from;
      g_debug ("Received forward id %s from %s", stanza_id, peer);
    }
  } else {
    // The server does not support MAM but we still need to handle history
    message = msg;
    peer = from;
  }
  g_debug ("Stealing parser for MAM, from %s at ID %s", peer, stanza_id);
  /**
   * Before we resume message processing we need to pre-cook the message.
   * If there's no body the whole server_got_stuff is skipped, so we may never
   * see the conversation. On the other hand making full parsing with html and
   * oob here is an overkill. Let's just try to fish end-state message from
   * the parser using signals which will override our empty message.
   */
  inflight_msg = g_new0(MamMsg, 1);
  inflight_msg->id = (char*)stanza_id;
  inflight_msg->p.who = (char*)peer;
  inflight_msg->p.flags = flags;
  if(stamp)
    inflight_msg->p.when = purple_str_to_time (stamp, TRUE, NULL, NULL, NULL);
  jabber_message_parse (js, message);
  if(stanza_id != NULL || inflight_msg->p.what != NULL)
    chatty_history_add_message (pc->account, &(inflight_msg->p),
                                (char**)&stanza_id, inflight_msg->type,
                                NULL);
  // Clear resubmission state
  if(peer != inflight_msg->p.who)
    g_free(inflight_msg->p.who);
  g_free(inflight_msg->p.what);
  g_free(inflight_msg);
  inflight_msg = NULL;
  // Stop processing, we have done that already
  return TRUE;
}


/**
 * chatty_mam_close:
 *
 * Unref node hashtable
 */
void
chatty_0313_close (void)
{
  PurplePlugin *jabber = chatty_xeps_get_jabber ();
  void *handle = chatty_xeps_get_handle ();
  purple_signal_disconnect (jabber,
                            "jabber-bare-info",
                            handle,
                            PURPLE_CALLBACK(cb_chatty_mam_bare_info));

  purple_signal_disconnect (jabber,
                            "jabber-receiving-message",
                            handle,
                            PURPLE_CALLBACK(cb_chatty_mam_msg_received));

  purple_signal_disconnect (purple_conversations_get_handle(),
                            "wrote-im-msg",
                            handle,
                            PURPLE_CALLBACK(cb_chatty_mam_msg_wrote));
  purple_signal_disconnect (purple_conversations_get_handle(),
                            "wrote-chat-msg",
                            handle,
                            PURPLE_CALLBACK(cb_chatty_mam_msg_wrote));
  g_hash_table_destroy (ht_bare_ctx);
}


/**
 * chatty_mam_init:
 *
 * Sets purple XEP functions
 * and defines libpurple signal callbacks
 *
 */
void
chatty_0313_init (void)
{
  PurplePlugin *jabber = chatty_xeps_get_jabber ();
  void *handle = chatty_xeps_get_handle ();
  ht_bare_ctx = g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       g_free,
                                       NULL);

  purple_signal_connect(jabber,
                        "jabber-bare-info",
                        handle,
                        PURPLE_CALLBACK(cb_chatty_mam_bare_info),
                        NULL);

  purple_signal_connect(jabber,
                        "jabber-receiving-message",
                        handle,
                        PURPLE_CALLBACK(cb_chatty_mam_msg_received),
                        NULL);

  purple_signal_connect(purple_conversations_get_handle(),
                        "receiving-im-msg",
                        handle,
                        PURPLE_CALLBACK(cb_chatty_mam_msg_receiving),
                        NULL);

  purple_signal_connect(chatty_conversations_get_handle(),
                        "conversation-write",
                        handle,
                        PURPLE_CALLBACK(cb_chatty_mam_msg_wrote),
                        NULL);
}
/* vim: set sts=2 et: */
