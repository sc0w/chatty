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
#define NS_DATA "jabber:x:data"
#define NS_RSM "http://jabber.org/protocol/rsm"

typedef struct {
  PurpleConvMessage p;
  char *id;
  PurpleConversationType type;
} MamMsg;

typedef struct {
  JabberStream  *js;
  char          *id;
  char          *to;
  char        *with;
  char       *after;
  char      *before;
  char       *start;
  char         *end;
  int           max;
} MAMQuery;

/* FIXME: What if purple becomes multithreaded 8-O */
typedef struct {
  GHashTable *qs;
  time_t   last_ts;
  MamMsg  *cur_msg;
  char    *cur_oid;
} MamCtx;

static GHashTable *ht_mam_ctx = NULL;

static void
mamq_free(void *ptr)
{
  MAMQuery *mamq = (MAMQuery*)ptr;
  if(ptr==NULL) return;
  g_free(mamq->id);
  g_free(mamq->to);
  g_free(mamq->with);
  g_free(mamq->after);
  g_free(mamq->before);
  g_free(mamq->start);
  g_free(mamq->end);
  g_free(mamq);
}

static void
mamm_free(void *ptr)
{
  MamMsg *mm = (MamMsg*)ptr;
  if(ptr==NULL) return;
  g_free(mm->id);
  g_free(mm->p.who);
  g_free(mm->p.what);
  g_free(mm->p.alias);
  g_free(mm);
}

/**
 * MAM Context Management API
 */
static void
mamc_free(void *ptr)
{
  MamCtx *mamc = (MamCtx*)ptr;
  if(ptr==NULL) return;
  g_free(mamc->cur_oid);
  mamm_free(mamc->cur_msg);
  g_hash_table_destroy(mamc->qs);
  g_free(mamc);
}

static MamCtx *
mamc_new(void)
{
  MamCtx *mamc = g_new0(MamCtx, 1);
  mamc->qs = g_hash_table_new_full(g_str_hash,
                                   g_str_equal,
                                   g_free,
                                   mamq_free);
  return mamc;
}

static MamCtx *
chatty_mam_ctx_get(PurpleAccount *pa)
{
  g_return_val_if_fail(pa != NULL, NULL);
  return g_hash_table_lookup(ht_mam_ctx, purple_account_get_username(pa));
}

static inline MamCtx *
chatty_mam_ctx_add(PurpleAccount *pa)
{
  MamCtx *mamc = chatty_mam_ctx_get(pa);
  g_return_val_if_fail(pa != NULL, NULL);
  if(mamc == NULL) {
    mamc = mamc_new();
    g_hash_table_insert(ht_mam_ctx,
                        g_strdup(purple_account_get_username(pa)), mamc);
  }
  return mamc;
}

static void
chatty_mam_ctx_del(PurpleAccount *pa)
{
  g_return_if_fail(pa != NULL);
  g_debug("Cleaning context for %s", purple_account_get_username(pa));
  g_hash_table_remove(ht_mam_ctx, purple_account_get_username(pa));
}

/**
 * MAM Query Handlers
 */
static void
cb_mam_query_prefs(JabberStream *js, const char *from,
                    JabberIqType type, const char *id,
                    xmlnode *res, gpointer data)
{
  PurpleAccount *pa = purple_connection_get_account(js->gc);
  xmlnode *prefs = xmlnode_get_child_with_namespace(res, "prefs", NS_MAMv2);
  const char *to = data;

  if(type == JABBER_IQ_RESULT && prefs) {
    const char *srv_def = xmlnode_get_attrib(prefs, "default");
    const char *clt_def = purple_account_get_ui_string(pa, CHATTY_UI,
                                              MAM_PREFS_DEF, MAM_DEF_ROSTER);
    if(g_strcmp0(clt_def, srv_def)) {
      JabberIq *iq = jabber_iq_new(js, JABBER_IQ_SET);
      prefs = xmlnode_new_child(iq->node, "prefs");
      xmlnode_set_namespace(prefs, NS_MAMv2);
      if(to != NULL)
        xmlnode_set_attrib(iq->node, "to", to);
      xmlnode_set_attrib(prefs, MAM_PREFS_DEF, clt_def);
      xmlnode_new_child(prefs,"always");
      xmlnode_new_child(prefs,"never");
      jabber_iq_send(iq);
    }
  }
}

static void
chatty_mam_query_prefs(PurpleConnection *pc, const char *to)
{
  JabberStream  *js = purple_connection_get_protocol_data (pc);
  JabberIq *iq = jabber_iq_new(js, JABBER_IQ_GET);
  xmlnode *prefs = xmlnode_new_child(iq->node, "prefs");
  xmlnode_set_namespace(prefs, NS_MAMv2);

  if(to != NULL)
    xmlnode_set_attrib(iq->node, "to", to);

  jabber_iq_set_callback(iq, cb_mam_query_prefs, (void*)to);

  jabber_iq_send(iq);
}

static void chatty_mam_query_archive (MAMQuery *mamq);

static void
cb_mam_query_result(JabberStream *js, const char *from,
                    JabberIqType type, const char *id,
                    xmlnode *res, gpointer data)
{
  xmlnode *fin = xmlnode_get_child_with_namespace(res, "fin", NS_MAMv2);
  PurpleAccount *pa = purple_connection_get_account(js->gc);
  MamCtx *mamc = chatty_mam_ctx_get(pa);
  MAMQuery *mamq = (MAMQuery*) data;

  if(type == JABBER_IQ_RESULT && fin != NULL) {
    const char *complete = xmlnode_get_attrib(fin, "complete");
    if(g_strcmp0(complete, "true")) {
      // not last page, need to continue
      xmlnode *set = xmlnode_get_child_with_namespace(fin, "set", NS_RSM);
      if(set) {
        xmlnode *last = xmlnode_get_child(set, "last");
        if(last) {
          g_free(mamq->after);
          mamq->after = xmlnode_get_data(last);
          chatty_mam_query_archive(mamq);
          return;
        }
      }
      fin = NULL; // Flag error state
    } else {
      g_debug("This is the last of them, standing down at %ld", mamc->last_ts);
      if(mamc->last_ts > 0)
        purple_account_set_int(pa, "mam_last_ts", mamc->last_ts);
    }
  } else {
      fin = NULL; // Flag error state
      // FIXME: restart if it's just item-not-found
  }
  if(fin == NULL) {
    // Report error and give up
    int strl;
    char *xml = xmlnode_to_str(res, &strl);
    g_debug("Error for MAM Query: %s", xml);
    g_free(xml);
  }
  // No follow up, clean up the context
  g_hash_table_remove(mamc->qs, mamq->id);
}

static void
chatty_mam_query_archive (MAMQuery *mamq)
{
  JabberIq  *iq;
  xmlnode   *mq;

  g_return_if_fail(mamq != NULL);
  g_return_if_fail(mamq->js != NULL);
  g_return_if_fail(mamq->id != NULL);

  iq = jabber_iq_new_query(mamq->js, JABBER_IQ_SET, NS_MAMv2);
  mq = xmlnode_get_child(iq->node, "query");

  xmlnode_set_attrib(mq, "queryid", mamq->id);
  if(mamq->to != NULL)
    xmlnode_set_attrib(iq->node, "to", mamq->to);
  jabber_iq_set_callback(iq, cb_mam_query_result, mamq);

  // Set search params
  if(mamq->with || mamq->start || mamq->end) {
    xmlnode *x = xmlnode_new_child(mq, "x");
    xmlnode *f = xmlnode_new_child(x, "field");
    xmlnode *v = xmlnode_new_child(f, "value");
    xmlnode_set_namespace(x, NS_DATA);
    xmlnode_set_attrib(x, "type", "submit");
    xmlnode_set_attrib(f, "type", "hidden");
    xmlnode_set_attrib(f, "var", "FORM_TYPE");
    xmlnode_insert_data(v, NS_MAMv2, -1);
    if(mamq->with) {
      f = xmlnode_new_child(x, "field");
      v = xmlnode_new_child(f, "value");
      xmlnode_set_attrib(f, "var", "with");
      xmlnode_insert_data(v, mamq->with, -1);
    }
    if(mamq->start) {
      f = xmlnode_new_child(x, "field");
      v = xmlnode_new_child(f, "value");
      xmlnode_set_attrib(f, "var", "start");
      xmlnode_insert_data(v, mamq->start, -1);
    }
    if(mamq->end) {
      f = xmlnode_new_child(x, "field");
      v = xmlnode_new_child(f, "value");
      xmlnode_set_attrib(f, "var", "end");
      xmlnode_insert_data(v, mamq->end, -1);
    }
  }

  if(mamq->before || mamq->after || (mamq->max > 0 && mamq->max < 1e9)) {
    xmlnode *rsm = xmlnode_new_child(mq, "set");
    xmlnode_set_namespace(rsm, NS_RSM);
    if(mamq->before) {
      xmlnode *v = xmlnode_new_child(rsm, "before");
      xmlnode_insert_data(v, mamq->before, -1);
    }
    if(mamq->after) {
      xmlnode *v = xmlnode_new_child(rsm, "after");
      xmlnode_insert_data(v, mamq->after, -1);
    }
    if(mamq->max > 0 && mamq->max < 1e9) {
      xmlnode *max = xmlnode_new_child(rsm, "max");
      char data[10];
      xmlnode_set_namespace(rsm, NS_RSM);
      snprintf(data, 10, "%d", mamq->max);
      xmlnode_insert_data(max, data, -1);
    }
  }

  jabber_iq_send(iq);
}

static void
cb_chatty_mam_bare_info (PurpleConnection *pc,
                          const char *bare,
                          const char *var)
{
  if(g_strcmp0(var, NS_MAMv2) == 0) {
    JabberStream  *js = purple_connection_get_protocol_data (pc);
    char *qid = jabber_get_next_id(js);
    GDateTime *dt;
    PurpleAccount *pa = purple_connection_get_account(pc);
    // Init CTX
    MamCtx *mamc = chatty_mam_ctx_add(pa);
    MAMQuery *mamq;

    if(g_strcmp0 (MAM_DEF_DISABLE,
                  purple_account_get_ui_string (pa, CHATTY_UI,
                                                MAM_PREFS_DEF, NULL)) == 0)
      return; // ok, if you say so

    mamq = g_new0(MAMQuery, 1);
    mamq->js = js;
    mamq->id = g_strdup(qid);
    if(g_strcmp0(bare, purple_account_get_username(pa)))
      mamq->to = g_strdup(bare);
    g_hash_table_insert(mamc->qs, qid, mamq);
    // Get last stop point
    mamc->last_ts = purple_account_get_int(pa, "mam_last_ts", 0);
    if(mamc->last_ts > 0) {
      dt = g_date_time_new_from_unix_utc(mamc->last_ts);
    } else {
      // last week should be good enough for the start
      GDateTime *now = g_date_time_new_now_utc();
      dt = g_date_time_add_days(now, -7);
      g_date_time_unref(now);
    }
    mamq->start = g_date_time_format(dt,"%FT%TZ");
    g_date_time_unref(dt);
    g_debug ("Server supports MAM %s on %s; Querying by %s from %s after %s",
                                    var, bare, qid, mamq->start, mamq->after);
    // Request MAM backlog
    chatty_mam_query_archive(mamq);
    // Also - request preferences and correct them if required
    chatty_mam_query_prefs(pc, mamq->to);
  }
}

static void
cb_chatty_mam_msg_wrote(PurpleAccount *pa, PurpleConvMessage *pcm,
                        char **uuid, PurpleConversationType type,
                        void *ctx)
{
  MamCtx *mamc = chatty_mam_ctx_get(pa);
  if(mamc == NULL)
    return;
  if(mamc->cur_oid && pcm->flags & PURPLE_MESSAGE_SEND) {
    // copy origin_id into uuid to be able to dedup outgoing messages
    *uuid = g_strdup(mamc->cur_oid);
    g_free(mamc->cur_oid);
    mamc->cur_oid = NULL;
    return;
  }

  if(mamc->cur_msg == NULL)
    return;

  // Skip non-jabber writes
  if(g_strcmp0 ("prpl-jabber", purple_account_get_protocol_id (pa)))
    return;

  mamc->cur_msg->p.what = g_strdup(pcm->what);
  // If flags are already set - enforce them, otherwise copy
  // Also always set NO_LOG flag to suppress in-app archiving
  if(mamc->cur_msg->p.flags) {
    pcm->flags = mamc->cur_msg->p.flags | PURPLE_MESSAGE_NO_LOG;
  } else {
    mamc->cur_msg->p.flags = pcm->flags;
    pcm->flags |= PURPLE_MESSAGE_NO_LOG;
  }
  mamc->cur_msg->type = type;
  mamc->cur_msg->p.alias = g_strdup(pcm->alias);
  mamc->cur_msg->p.when = pcm->when;
  mamc->cur_msg->p.who = g_strdup(pcm->who);
  if(mamc->cur_msg->id)
    *uuid = g_strdup(mamc->cur_msg->id);
  g_debug ("Received message on %s of type %d with flags %d",
            mamc->cur_msg->p.alias, mamc->cur_msg->type, mamc->cur_msg->p.flags);
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
  PurpleAccount *pa = purple_connection_get_account (pc);
  MamCtx *mamc = chatty_mam_ctx_add(pa);

  if (msg == NULL)
    return FALSE;

  node_result = xmlnode_get_child_with_namespace (msg, "result", NS_MAMv2);
  node_sid    = xmlnode_get_child_with_namespace (msg, "stanza-id", NS_SIDv0);

  if(mamc->cur_msg != NULL) {
    // Skip resubmission - break the loop
    g_debug ("Received resubmission %s %s %s", id, from, to);
    return FALSE;
  }

  if(node_result != NULL || node_sid != NULL) {
    if(node_result != NULL) {
      xmlnode    *node_fwd;
      xmlnode    *node_delay;
      const char *msg_type;
      int dts;
      query_id = xmlnode_get_attrib (node_result, "queryid");
      stanza_id = xmlnode_get_attrib (node_result, "id");
      user = purple_account_get_username(purple_connection_get_account(pc));

      // Check result and query-id are valid
      if(query_id == NULL || !g_hash_table_contains(mamc->qs, query_id)) {
        // Fake result injection?
        g_debug ("Fake MAM result[%s] injection from %s", query_id, from);
        return FALSE;
      }

      node_fwd = xmlnode_get_child_with_namespace (node_result, "forwarded", NS_FWDv0);
      if(node_fwd == NULL) // this is rather unexpected, yield
        return FALSE;

      message = xmlnode_get_child (node_fwd, "message");
      if(message == NULL)
        return FALSE; // Now this is bizare

      node_delay = xmlnode_get_child (node_fwd, "delay");
      peer = xmlnode_get_attrib (message, "from");
      if(node_delay != NULL) {
        stamp = xmlnode_get_attrib (node_delay, "stamp");
        /* Copy delay down for the parser */
        xmlnode_insert_child (message, xmlnode_copy (node_delay));
        if(stamp)
          mamc->last_ts = purple_str_to_time (stamp, TRUE, NULL, NULL, NULL);
      }
      // check history and drop the dup
      msg_type = xmlnode_get_attrib(message, "type");
      if(from && msg_type && g_strcmp0(msg_type, "groupchat") == 0) {
        dts = get_chat_timestamp_for_uuid(stanza_id, from);
      } else {
        dts = get_im_timestamp_for_uuid(stanza_id, user);
      }
      if(dts < INT_MAX) {
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
          peer = xmlnode_get_attrib (message, "from");
        }
        g_free (bare_peer);
      } else {
        xmlnode_set_attrib (message, "from", xmlnode_get_attrib (message, "to"));
        peer = xmlnode_get_attrib (message, "from");
        flags |= PURPLE_MESSAGE_SEND;
      }
      if(flags & PURPLE_MESSAGE_SEND) {
        // For sent messages need to attempt dedup based on origin-id
        xmlnode *node_oid = xmlnode_get_child_with_namespace (message, "origin-id", NS_SIDv0);
        if(node_oid) {
          const char *uuid = xmlnode_get_attrib (node_oid, "id");
          if(uuid) {
            dts = get_im_timestamp_for_uuid(uuid, user);
            if(dts < INT_MAX) {
              g_debug ("Message id %s for acc %s is already stored on %d", uuid, user, dts);
              return TRUE; // note - true means stop processing
            }
          }
        }
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
  mamc->cur_msg = g_new0(MamMsg, 1);
  mamc->cur_msg->id = (char*)stanza_id;
  mamc->cur_msg->p.who = (char*)peer;
  mamc->cur_msg->p.flags = flags;
  if(stamp)
    mamc->cur_msg->p.when = mamc->last_ts;
  jabber_message_parse (js, message);
  if(stanza_id != NULL || mamc->cur_msg->p.what != NULL)
    chatty_history_add_message (pc->account, &(mamc->cur_msg->p),
                                (char**)&stanza_id, mamc->cur_msg->type,
                                NULL);
  // Clear resubmission state
  if(peer == mamc->cur_msg->p.who)
    mamc->cur_msg->p.who = NULL;
  mamc->cur_msg->id = NULL;
  mamm_free(mamc->cur_msg);
  mamc->cur_msg = NULL;
  // Stop processing, we have done that already
  return TRUE;
}

/**
 * cb_chatty_mam_xmlnode_send:
 * @pc: a PurpleConnection
 * @packet: a xmlnode
 *
 * This function is called via the
 * "jabber-sending-xmlnode" signal
 *
 */
static void
cb_chatty_mam_xmlnode_send (PurpleConnection  *pc,
                            xmlnode          **packet,
                            gpointer           null)
{
  xmlnode    *node_body;
  xmlnode    *node_id;

  if (*packet && (*packet)->name && g_strcmp0((*packet)->name, "message") == 0) {
    node_body = xmlnode_get_child (*packet, "body");

    if (node_body) {
      MamCtx *mamc = chatty_mam_ctx_get(purple_connection_get_account(pc));
      if(mamc == NULL)
        return;
      node_id = xmlnode_new_child (*packet, "origin-id");
      xmlnode_set_namespace (node_id, NS_SIDv0);
      g_free(mamc->cur_oid);
      chatty_utils_generate_uuid(&(mamc->cur_oid));
      xmlnode_set_attrib(node_id, "id", mamc->cur_oid);

      g_debug ("Set origin-id %s for outgoing message", mamc->cur_oid);
    }
  }
}

static void
cb_chatty_mam_disconnect(PurpleConnection *pc, gpointer ptr)
{
  chatty_mam_ctx_del(purple_connection_get_account(pc));
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
  g_hash_table_destroy (ht_mam_ctx);
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
  ht_mam_ctx = g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       g_free,
                                       mamc_free);

  purple_signal_connect(jabber,
                        "jabber-bare-info",
                        handle,
                        PURPLE_CALLBACK(cb_chatty_mam_bare_info),
                        NULL);

  purple_signal_connect(jabber,
                        "jabber-sending-xmlnode",
                        handle,
                        PURPLE_CALLBACK(cb_chatty_mam_xmlnode_send),
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
  purple_signal_connect(purple_connections_get_handle(),
                        "signed-off",
                        handle,
                        PURPLE_CALLBACK(cb_chatty_mam_disconnect),
                        NULL);
}
/* vim: set sts=2 et: */
