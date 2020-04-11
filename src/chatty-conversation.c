/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#define G_LOG_DOMAIN "chatty-conversation"

#include <glib/gi18n.h>
#include "chatty-window.h"
#include "chatty-manager.h"
#include "chatty-icons.h"
#include "chatty-purple-init.h"
#include "chatty-message-list.h"
#include "chatty-conversation.h"
#include "chatty-history.h"
#include "chatty-utils.h"
#include "chatty-notify.h"
#include "chatty-chat-view.h"

#define LIBFEEDBACK_USE_UNSTABLE_API
#include <libfeedback.h>

#define LAZY_LOAD_MSGS_LIMIT 12
#define LAZY_LOAD_INITIAL_MSGS_LIMIT 20
#define MAX_TIMESTAMP_SIZE 256

static void
chatty_conv_write_conversation (PurpleConversation *conv,
                                const char         *who,
                                const char         *alias,
                                const char         *message,
                                PurpleMessageFlags  flags,
                                time_t              mtime);


void chatty_conv_new (PurpleConversation *conv);
static PurpleBlistNode *chatty_get_conv_blist_node (PurpleConversation *conv);
static void chatty_conv_conversation_update (PurpleConversation *conv);


// *** callbacks

static void
cb_buddy_typing (PurpleAccount *account,
                 const char    *name)
{
  PurpleConversation *conv;
  ChattyConversation *chatty_conv;

  conv = purple_find_conversation_with_account (PURPLE_CONV_TYPE_IM,
                                                name,
                                                account);
  if (!conv) {
    return;
  }

  chatty_conv = CHATTY_CONVERSATION(conv);

  if (chatty_conv && chatty_conv->conv == conv) {
    chatty_msg_list_show_typing_indicator (chatty_conv->msg_list);
  }
}


static void
cb_buddy_typed (PurpleAccount *account,
                const char    *name)
{
  PurpleConversation *conv;
  ChattyConversation *chatty_conv;

  conv = purple_find_conversation_with_account (PURPLE_CONV_TYPE_IM,
                                                name,
                                                account);
  if (!conv) {
    return;
  }

  chatty_conv = CHATTY_CONVERSATION(conv);

  if (chatty_conv && chatty_conv->conv == conv) {
    chatty_msg_list_hide_typing_indicator (chatty_conv->msg_list);
  }
}


static void
cb_update_buddy_icon (PurpleBuddy *buddy)
{
  PurpleConversation *conv;

  conv = purple_find_conversation_with_account (PURPLE_CONV_TYPE_IM, 
                                                buddy->name, 
                                                buddy->account);

  if (conv) {
    chatty_conv_conversation_update (conv);
  }
}

// *** end callbacks


static PurpleCmdRet
cb_chatty_cmd (PurpleConversation  *conv,
               const gchar         *cmd,
               gchar              **args,
               gchar              **error,
               void                *data)
{
  ChattySettings *settings;
  char *msg = NULL;

  settings = chatty_settings_get_default ();

  if (args[0] == NULL || !g_strcmp0 (args[0], "help")) {
    msg = g_strdup ("Commands for setting properties:\n\n"
                    "General settings:\n"
                    " - '/chatty help': Displays this message.\n"
                    " - '/chatty emoticons [on; off]': Convert emoticons\n"
                    " - '/chatty return_sends [on; off]': Return = send message\n"
                    "\n"
                    "XMPP settings:\n"
                    " - '/chatty grey_offline [on; off]': Greyout offline-contacts\n"
                    " - '/chatty blur_idle [on; off]': Blur idle-contacts icons\n"
                    " - '/chatty typing_info [on; off]': Send typing notifications\n"
                    " - '/chatty msg_receipts [on; off]': Send message receipts\n"
                    " - '/chatty msg_carbons [on; off]': Share chat history\n");
  } else if (!g_strcmp0 (args[1], "on")) {
    if (!g_strcmp0 (args[0], "return_sends")) {
      g_object_set (settings, "return-sends-message", TRUE, NULL);
      msg = g_strdup ("Return key sends messages");
    } else if (!g_strcmp0 (args[0], "grey_offline")) {
      g_object_set (settings, "greyout-offline-buddies", TRUE, NULL);
      msg = g_strdup ("Offline user avatars will be greyed out");
    } else if (!g_strcmp0 (args[0], "blur_idle")) {
      g_object_set (settings, "blur-idle-buddies", TRUE, NULL);
      msg = g_strdup ("Offline user avatars will be blurred");
    } else if (!g_strcmp0 (args[0], "typing_info")) {
      g_object_set (settings, "send-typing", TRUE, NULL);
      msg = g_strdup ("Typing messages will be sent");
    } else if (!g_strcmp0 (args[0], "msg_receipts")) {
      g_object_set (settings, "send-receipts", TRUE, NULL);
      msg = g_strdup ("Message receipts will be sent");
    } else if (!g_strcmp0 (args[0], "msg_carbons")) {
      g_object_set (settings, "message-carbons", TRUE, NULL);
      msg = g_strdup ("Chat history will be shared");
    } else if (!g_strcmp0 (args[0], "emoticons")) {
      g_object_set (settings, "convert-emoticons", TRUE, NULL);
      msg = g_strdup ("Emoticons will be converted");
    } else if (!g_strcmp0 (args[0], "welcome")) {
      g_object_set (settings, "first-start", TRUE, NULL);
      msg = g_strdup ("Welcome screen has been reset");
    }
  } else if (!g_strcmp0 (args[1], "off")) {
    if (!g_strcmp0 (args[0], "return_sends")) {
      g_object_set (settings, "return-sends-message", FALSE, NULL);
      msg = g_strdup ("Return key doesn't send messages");
    } else if (!g_strcmp0 (args[0], "grey_offline")) {
      g_object_set (settings, "greyout-offline-buddies", FALSE, NULL);
      msg = g_strdup ("Offline user avatars will not be greyed out");
    } else if (!g_strcmp0 (args[0], "blur_idle")) {
      g_object_set (settings, "blur-idle-buddies", FALSE, NULL);
      msg = g_strdup ("Offline user avatars will not be blurred");
    } else if (!g_strcmp0 (args[0], "typing_info")) {
      g_object_set (settings, "send-typing", FALSE, NULL);
      msg = g_strdup ("Typing messages will be hidden");
    } else if (!g_strcmp0 (args[0], "msg_receipts")) {
      g_object_set (settings, "send-receipts", FALSE, NULL);
      msg = g_strdup ("Message receipts won't be sent");
    } else if (!g_strcmp0 (args[0], "msg_carbons")) {
      g_object_set (settings, "message-carbons", FALSE, NULL);
      msg = g_strdup ("Chat history won't be shared");
    } else if (!g_strcmp0 (args[0], "emoticons")) {
      g_object_set (settings, "convert-emoticons", FALSE, NULL);
      msg = g_strdup ("emoticons will not be converted");
    }
  }

  g_debug("@DEBUG@ cb_chatty_cmd");
  g_debug("%s", args[0]);

  if (msg) {
    purple_conversation_write (conv,
                               "chatty",
                               msg,
                               PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG,
                               time(NULL));

    g_free (msg);
  }

  return PURPLE_CMD_RET_OK;
}


/**
 * chatty_conv_set_unseen:
 * @chatty_conv: a ChattyConversation
 * @state: a ChattyUnseenState
 *
 * Sets the seen/unseen state of a conversation
 *
 */
void
chatty_conv_set_unseen (ChattyConversation *chatty_conv,
                        ChattyUnseenState   state)
{
  if (state == CHATTY_UNSEEN_NONE)
  {
    chatty_conv->unseen_count = 0;
    chatty_conv->unseen_state = CHATTY_UNSEEN_NONE;
  }
  else
  {
    if (state >= CHATTY_UNSEEN_TEXT)
      chatty_conv->unseen_count++;

    if (state > chatty_conv->unseen_state)
      chatty_conv->unseen_state = state;
  }

  purple_conversation_set_data (chatty_conv->conv, "unseen-count",
                                GINT_TO_POINTER(chatty_conv->unseen_count));

  purple_conversation_set_data (chatty_conv->conv, "unseen-state",
                                GINT_TO_POINTER(chatty_conv->unseen_state));

  purple_conversation_update (chatty_conv->conv, PURPLE_CONV_UPDATE_UNSEEN);
}


/**
 * chatty_conv_find_unseen:
 * @state: a ChattyUnseenState
 *
 * Fills a GList with unseen IM conversations
 *
 * Returns: GList
 *
 */
GList *
chatty_conv_find_unseen (ChattyUnseenState  state)
{
  GList *l;
  GList *r = NULL;
  guint  c = 0;

  l = purple_get_ims();

  for (; l != NULL; l = l->next) {
    PurpleConversation *conv = (PurpleConversation*)l->data;
    ChattyConversation *chatty_conv = CHATTY_CONVERSATION(conv);

    if(chatty_conv == NULL || chatty_conv->conv != conv) {
      continue;
    }

    if (chatty_conv->unseen_state == state) {
      r = g_list_prepend(r, conv);
      c++;
    }
  }

  return r;
}


/**
 * chatty_conv_container_get_active_chatty_conv:
 * @notebook: a GtkNotebook
 *
 * Returns the chatty conversation that is
 * currently set active in the notebook
 *
 * Returns: ChattyConversation
 *
 */
ChattyConversation *
chatty_conv_container_get_active_chatty_conv (GtkNotebook *notebook)
{
  int       index;
  GtkWidget *tab_cont;

  index = gtk_notebook_get_current_page (GTK_NOTEBOOK(notebook));

  if (index == -1) {
    index = 0;
  }

  tab_cont = gtk_notebook_get_nth_page (GTK_NOTEBOOK(notebook), index);

  if (!tab_cont) {
    return NULL;
  }

  return g_object_get_data (G_OBJECT(tab_cont), "ChattyConversation");
}


/**
 * chatty_conv_muc_list_add_users:
 * @conv:        a ChattyConversation
 * @buddies:     a Glist
 * @new_arrivals a gboolean
 *
 * Add users to the muc list
 *
 * invoked from PurpleConversationUiOps
 *
 */
static void
chatty_conv_muc_list_add_users (PurpleConversation *conv,
                                GList              *users,
                                gboolean            new_arrivals)
{
  ChattyChat *chat;

  chat = chatty_manager_find_purple_conv (chatty_manager_get_default (), conv);

  if (chat)
    chatty_chat_add_users (chat, users);
}


/**
 * chatty_conv_muc_list_update_user:
 * @conv:  a PurpleConversation
 * @users: a Glist
 *
 * Update user in muc list
 *
 * invoked from PurpleConversationUiOps
 *
 */
static void
chatty_conv_muc_list_update_user (PurpleConversation *conv,
                                  const char         *user)
{
  ChattyChat *chat;

  chat = chatty_manager_find_purple_conv (chatty_manager_get_default (), conv);
  g_return_if_fail (chat);

  chatty_chat_emit_user_changed (chat, user);
}


/**
 * chatty_conv_stack_add_conv:
 * @conv: a ChattyConversation
 *
 * Add a ChattyConversation to the
 * conversations stack
 *
 */
static void
chatty_conv_stack_add_conv (ChattyConversation *chatty_conv)
{
  ChattyWindow            *window;
  PurpleConversation      *conv = chatty_conv->conv;
  GtkWidget               *convs_notebook;
  const gchar             *tab_txt;
  gchar                   *text;
  gchar                   **name_split;

  window = chatty_utils_get_window ();

  convs_notebook = chatty_window_get_convs_notebook (window);

  tab_txt = purple_conversation_get_title (conv);

  gtk_notebook_append_page (GTK_NOTEBOOK(convs_notebook),
                            chatty_conv->tab_cont, NULL);

  name_split = g_strsplit (tab_txt, "@", -1);
  text = g_strdup_printf ("%s %s",name_split[0], " >");

  gtk_notebook_set_tab_label_text (GTK_NOTEBOOK(convs_notebook),
                                   chatty_conv->tab_cont, text);

  gtk_widget_show (chatty_conv->tab_cont);

  gtk_notebook_set_current_page (GTK_NOTEBOOK(convs_notebook), 0);

  if (purple_prefs_get_bool (CHATTY_PREFS_ROOT "/conversations/show_tabs")) {
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK(convs_notebook), TRUE);
  } else {
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK(convs_notebook), FALSE);
  }

  g_free (text);
  g_strfreev (name_split);

  chatty_chat_view_focus_entry (CHATTY_CHAT_VIEW (chatty_conv->chat_view));
}


/**
 * chatty_conv_find_conv:
 * @conv: a PurpleConversation
 *
 * Find the Chatty-GUI for a given PurpleConversation
 *
 * Returns: A ChattyConversation
 *
 */
static ChattyConversation *
chatty_conv_find_conv (PurpleConversation * conv)
{
  PurpleBuddy     *buddy;
  PurpleContact   *contact;
  PurpleBlistNode *contact_node,
                  *buddy_node;

  buddy = purple_find_buddy (conv->account, conv->name);

  if (!buddy)
    return NULL;

  if (!(contact = purple_buddy_get_contact (buddy)))
    return NULL;

  contact_node = PURPLE_BLIST_NODE (contact);

  for (buddy_node = purple_blist_node_get_first_child (contact_node);
       buddy_node;
       buddy_node = purple_blist_node_get_sibling_next (buddy_node)) {
    PurpleBuddy *b = PURPLE_BUDDY (buddy_node);
    PurpleConversation *c;

    c = purple_find_conversation_with_account (PURPLE_CONV_TYPE_IM,
                                               b->name,
                                               b->account);
    if (!c)
        continue;
    if (c->ui_data)
        return c->ui_data;
  }

  return NULL;
}


/**
 * chatty_conv_write_chat:
 * @conv:     a PurpleConversation
 * @who:      the buddy name
 * @message:  the message text
 * @flags:    PurpleMessageFlags
 * @mtime:    mtime
 *
 * Send an instant message
 *
 */
static void
chatty_conv_write_chat (PurpleConversation *conv,
                        const char         *who,
                        const char         *message,
                        PurpleMessageFlags  flags,
                        time_t              mtime)
{
  purple_conversation_write (conv, who, message, flags, mtime);

}


/**
 * chatty_conv_write_im:
 * @conv:     a PurpleConversation
 * @who:      the buddy name
 * @message:  the message text
 * @flags:    PurpleMessageFlags
 * @mtime:    mtime
 *
 * Send an instant message
 *
 */
static void
chatty_conv_write_im (PurpleConversation *conv,
                      const char         *who,
                      const char         *message,
                      PurpleMessageFlags  flags,
                      time_t              mtime)
{
  ChattyConversation *chatty_conv;

  chatty_conv = CHATTY_CONVERSATION (conv);

  if (conv != chatty_conv->conv &&
      flags & PURPLE_MESSAGE_ACTIVE_ONLY)
  {
    return;
  }

  purple_conversation_write (conv, who, message, flags, mtime);
}


static void
on_feedback_triggered (LfbEvent      *event,
		       GAsyncResult  *res,
		       LfbEvent     **cmp)
{
  g_autoptr (GError) err = NULL;

  g_return_if_fail (LFB_IS_EVENT (event));

  if (!lfb_event_trigger_feedback_finish (event, res, &err)) {
    g_warning ("Failed to trigger feedback for %s",
	       lfb_event_get_event (event));
  }
}


/**
 * chatty_conv_write_conversation:
 * @conv:     a PurpleConversation
 * @who:      the buddy name
 * @alias:    the buddy alias
 * @message:  the message text
 * @flags:    PurpleMessageFlags
 * @mtime:    mtime
 *
 * The function is called from the
 * struct 'PurpleConversationUiOps'
 * when a message was sent or received
 *
 */
static void
chatty_conv_write_conversation (PurpleConversation *conv,
                                const char         *who,
                                const char         *alias,
                                const char         *message,
                                PurpleMessageFlags  flags,
                                time_t              mtime)
{
  ChattyConversation       *chatty_conv;
  ChattyConversation       *active_chatty_conv;
  PurpleConversationType    type;
  PurpleConnection         *gc;
  PurplePluginProtocolInfo *prpl_info = NULL;
  PurpleAccount            *account;
  PurpleBuddy              *buddy = NULL;
  PurpleBlistNode          *node;
  gboolean                  group_chat = TRUE;
  gboolean                  conv_active;
  GdkPixbuf                *avatar = NULL;
  GtkWidget                *icon = NULL;
  ChattyWindow             *window;
  GtkWidget                *convs_notebook;
  int                       chat_id;
  const char               *buddy_name;
  gchar                    *titel;
  g_autofree char          *uuid = NULL;
  g_autofree char          *timestamp = NULL;
  PurpleConvMessage        pcm = {
                            NULL,
                            NULL,
                            flags,
                            mtime,
                            conv,
                            NULL};
  g_autoptr(GError)         err = NULL;
  g_autoptr(LfbEvent)       event = NULL;

  chatty_conv = CHATTY_CONVERSATION (conv);

  g_return_if_fail (chatty_conv != NULL);

  if ((flags & PURPLE_MESSAGE_SYSTEM) && !(flags & PURPLE_MESSAGE_NOTIFY)) {
    flags &= ~(PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV);
  }

  node = chatty_get_conv_blist_node (conv);

  account = purple_conversation_get_account (conv);
  g_return_if_fail (account != NULL);
  gc = purple_account_get_connection (account);
  g_return_if_fail (gc != NULL || !(flags & (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV)));

  type = purple_conversation_get_type (conv);

  if (type == PURPLE_CONV_TYPE_CHAT)
  {
    prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

    if (prpl_info && prpl_info->get_cb_real_name) {
      chat_id = purple_conv_chat_get_id (PURPLE_CONV_CHAT(conv));

      pcm.who = prpl_info->get_cb_real_name(gc, chat_id, who);

      if (pcm.who) {
        const char *color;
        buddy = purple_find_buddy (account, pcm.who);
        color = chatty_utils_get_color_for_str (pcm.who);

        avatar = chatty_icon_get_buddy_icon ((PurpleBlistNode*)buddy,
                                             alias,
                                             CHATTY_ICON_SIZE_MEDIUM,
                                             color,
                                             FALSE);
        if (avatar) {
          icon = gtk_image_new_from_pixbuf (avatar);
          g_object_unref (avatar);
        }
      }
    }
  } else {
    buddy = purple_find_buddy (account, who);
    node = (PurpleBlistNode*)buddy;

    if (node) {
      purple_blist_node_set_bool (node, "chatty-autojoin", TRUE);
    }

    group_chat = FALSE;
    pcm.who = chatty_utils_jabber_id_strip(who);
  }

  // No reason to go further if we ignore system/status
  if (flags & PURPLE_MESSAGE_SYSTEM &&
      type == PURPLE_CONV_TYPE_CHAT &&
      ! purple_blist_node_get_bool (node, "chatty-status-msg"))
  {
    g_debug("Skipping status[%d] message[%s] for %s <> %s", flags,
            message, purple_account_get_username(account), pcm.who);
    // FIXME: Dunno why but without that it segfaults on first skip :(
    chatty_conv_set_unseen (chatty_conv, CHATTY_UNSEEN_NONE);
    g_free(pcm.who);
    return;
  }

  timestamp  = g_malloc0 (MAX_TIMESTAMP_SIZE * sizeof(char));
  if (!strftime (timestamp, MAX_TIMESTAMP_SIZE * sizeof(char),
                  (time(NULL) - mtime < 86400) ? "%R" : "%c",
                  localtime(&mtime)))
  {
    timestamp = g_strdup("00:00");
  }

  pcm.what = g_strdup(message);
  pcm.alias = g_strdup(purple_conversation_get_name (conv));

  // If anyone wants to suppress archiving - feel free to set NO_LOG flag
  purple_signal_emit (chatty_conversations_get_handle(),
                      "conversation-write", account, &pcm, &uuid, type);
  g_debug("Posting mesage id:%s flags:%d type:%d from:%s",
                                    uuid, pcm.flags, type, pcm.who);

  if (*message != '\0') {

    if (pcm.flags & PURPLE_MESSAGE_SYSTEM) {
      // System is usually also RECV so should be first to catch
      chatty_msg_list_add_message (chatty_conv->msg_list,
                                   MSG_IS_SYSTEM,
                                   message,
                                   NULL,
                                   NULL);
    } else if (pcm.flags & PURPLE_MESSAGE_RECV) {

      window = chatty_utils_get_window ();

      convs_notebook = chatty_window_get_convs_notebook (window);

      active_chatty_conv = chatty_conv_container_get_active_chatty_conv (GTK_NOTEBOOK(convs_notebook));

      conv_active = (chatty_conv == active_chatty_conv && gtk_widget_is_drawable (convs_notebook));

      if (buddy && purple_blist_node_get_bool (node, "chatty-notifications") && !conv_active) {

        event = lfb_event_new("message-new-instant");
        lfb_event_trigger_feedback_async (event, NULL,
                                          (GAsyncReadyCallback)on_feedback_triggered,
                                          NULL);

        buddy_name = purple_buddy_get_alias (buddy);

        titel = g_strdup_printf (_("New message from %s"), buddy_name);

        avatar = chatty_icon_get_buddy_icon ((PurpleBlistNode*)buddy,
                                              alias,
                                              CHATTY_ICON_SIZE_SMALL,
                                              chatty_blist_protocol_is_sms (account) ?
                                              CHATTY_COLOR_GREEN : CHATTY_COLOR_BLUE,
                                              FALSE);

        chatty_notify_show_notification (titel, message, CHATTY_NOTIFY_MESSAGE_RECEIVED, conv, avatar);

        g_object_unref (avatar);

        g_free (titel);
      }

      chatty_msg_list_add_message (chatty_conv->msg_list,
                                   MSG_IS_INCOMING,
                                   message,
                                   group_chat ? who : timestamp,
                                   icon ? icon : NULL);

    } else if (flags & PURPLE_MESSAGE_SEND && pcm.flags & PURPLE_MESSAGE_SEND) {
      // normal send
      chatty_msg_list_add_message (chatty_conv->msg_list,
                                   MSG_IS_OUTGOING,
                                   message,
                                   NULL,
                                   NULL);

    } else if (pcm.flags & PURPLE_MESSAGE_SEND) {
      // offline send (from MAM)
      // FIXME: current list_box does not allow ordering rows by timestamp
      // TODO: Needs proper sort function and timestamp as user_data for rows
      // FIXME: Alternatively may need to reload history to re-populate rows
      chatty_msg_list_add_message (chatty_conv->msg_list,
                                   MSG_IS_OUTGOING,
                                   message,
                                   timestamp,
                                   NULL);

    }

    if (chatty_conv->oldest_message_displayed == NULL)
      chatty_conv->oldest_message_displayed = g_steal_pointer(&uuid);

    chatty_conv_set_unseen (chatty_conv, CHATTY_UNSEEN_NONE);
  }

  g_free (pcm.who);
  g_free (pcm.what);
  g_free (pcm.alias);
}


/**
 * chatty_get_conv_blist_node:
 * @conv: a PurpleConversation
 *
 * Returns the buddy node for the
 * given conversation
 *
 * Returns: a PurpleBlistNode
 *
 */
static PurpleBlistNode *
chatty_get_conv_blist_node (PurpleConversation *conv)
{
  PurpleBlistNode *node = NULL;

  switch (purple_conversation_get_type (conv)) {
    case PURPLE_CONV_TYPE_IM:
      node = PURPLE_BLIST_NODE (purple_find_buddy (conv->account,
                                                   conv->name));
      break;
    case PURPLE_CONV_TYPE_CHAT:
      node = PURPLE_BLIST_NODE (purple_blist_find_chat (conv->account,
                                                        conv->name));
      break;
    case PURPLE_CONV_TYPE_UNKNOWN:
    case PURPLE_CONV_TYPE_MISC:
    case PURPLE_CONV_TYPE_ANY:
    default:
      g_warning ("Unhandled converstation type %d",
                 purple_conversation_get_type (conv));
      break;
  }
  return node;
}


/**
 * chatty_conv_switch_conv:
 * @chatty_conv: a ChattyConversation
 *
 * Brings the conversation-pane of chatty_conv to
 * the front
 *
 */
static void
chatty_conv_switch_conv (ChattyConversation *chatty_conv)
{
  ChattyWindow           *window;
  PurpleConversationType  conv_type;
  GtkWidget              *convs_notebook;
  gint                    page_num;

  window = chatty_utils_get_window ();

  convs_notebook = chatty_window_get_convs_notebook (window);

  conv_type = purple_conversation_get_type (chatty_conv->conv);

  page_num = gtk_notebook_page_num (GTK_NOTEBOOK(convs_notebook),
                                    chatty_conv->tab_cont);

  gtk_notebook_set_current_page (GTK_NOTEBOOK(convs_notebook),
                                 page_num);

  g_debug ("chatty_conv_switch_conv active_conv: %s   page_num %i",
           purple_conversation_get_name (chatty_conv->conv), page_num);

  if (conv_type == PURPLE_CONV_TYPE_CHAT) {
    chatty_window_set_header_chat_info_button_visible (window, TRUE);
  }

  chatty_chat_view_focus_entry (CHATTY_CHAT_VIEW (chatty_conv->chat_view));
}


/**
 * chatty_conv_remove_conv:
 * @chatty_conv: a ChattyConversation
 *
 * Remove the conversation-pane of chatty_conv
 *
 */
static void
chatty_conv_remove_conv (ChattyConversation *chatty_conv)
{
  ChattyWindow  *window;
  GtkWidget     *convs_notebook;
  guint          index;

  window = chatty_utils_get_window ();

  convs_notebook = chatty_window_get_convs_notebook (window);

  index = gtk_notebook_page_num (GTK_NOTEBOOK(convs_notebook),
                                 chatty_conv->tab_cont);

  gtk_notebook_remove_page (GTK_NOTEBOOK(convs_notebook), index);

  g_debug ("chatty_conv_remove_conv conv");
}


/**
 * chatty_conv_present_conversation:
 * @conv: a PurpleConversation
 *
 * Makes #conv the active conversation and
 * presents it to the user.
 *
 */
static void
chatty_conv_present_conversation (PurpleConversation *conv)
{
  ChattyConversation *chatty_conv;

  chatty_conv = CHATTY_CONVERSATION (conv);

  g_debug ("chatty_conv_present_conversation conv: %s", purple_conversation_get_name (conv));

  chatty_conv_switch_conv (chatty_conv);
}


/**
 * chatty_conv_im_with_buddy:
 * @account: a PurpleAccount
 * @name: the buddy name
 *
 * Starts a new conversation with a buddy.
 * If there is already an instance of the conversation
 * the GUI presents it to the user.
 *
 */
void
chatty_conv_im_with_buddy (PurpleAccount *account,
                           const char    *name)
{
  PurpleConversation *conv;

  g_return_if_fail (purple_account_is_connected (account));
  g_return_if_fail (name != NULL);

  conv = purple_find_conversation_with_account (PURPLE_CONV_TYPE_IM,
                                                name,
                                                account);

  if (conv == NULL) {
    conv = purple_conversation_new (PURPLE_CONV_TYPE_IM,
                                    account,
                                    name);
  }

  chatty_conv_show_conversation (conv);
}


/**
 * chatty_conv_conversation_update:
 * @conv: a PurpleConversation
 *
 * Update conversation UI
 *
 */
static void
chatty_conv_conversation_update (PurpleConversation *conv)
{
  ChattyWindow    *window;
  PurpleAccount   *account;
  PurpleBuddy     *buddy;
  PurpleContact   *contact;
  GdkPixbuf       *avatar;
  g_autofree char *name = NULL;
  const char      *buddy_alias;
  const char      *contact_alias;

  if (!conv) {
    return;
  }

  window = chatty_utils_get_window ();

  account = purple_conversation_get_account (conv);
  name = chatty_utils_jabber_id_strip (purple_conversation_get_name (conv));
  buddy = purple_find_buddy (account, name);
  buddy_alias = purple_buddy_get_alias (buddy);

  avatar = chatty_icon_get_buddy_icon (PURPLE_BLIST_NODE(buddy),
                                       name,
                                       CHATTY_ICON_SIZE_SMALL,
                                       chatty_blist_protocol_is_sms (account) ?
                                       CHATTY_COLOR_GREEN : CHATTY_COLOR_BLUE,
                                       FALSE);

  contact = purple_buddy_get_contact (buddy);
  contact_alias = purple_contact_get_alias (contact);

  chatty_window_update_sub_header_titlebar (window,
                                            avatar, 
                                            contact_alias ? contact_alias : buddy_alias);

  g_object_unref (avatar);
}



/**
 * chatty_conv_show_conversation:
 * @conv: a PurpleConversation
 *
 * Shows a conversation after a notification
 *
 * Called from cb_open_message in chatty-notify.c
 *
 */
void
chatty_conv_show_conversation (PurpleConversation *conv)
{
  ChattyWindow       *window;
  ChattyConversation *chatty_conv;

  if (!conv) {
    return;
  }

  window = chatty_utils_get_window ();

  chatty_conv = CHATTY_CONVERSATION (conv);

  chatty_conv_present_conversation (conv);
  chatty_conv_set_unseen (chatty_conv, CHATTY_UNSEEN_NONE);

  chatty_conv_conversation_update (conv);

  chatty_window_change_view (window, CHATTY_VIEW_MESSAGE_LIST);

  gtk_window_present (GTK_WINDOW(window));
}


void
chatty_conv_add_history_since_component (GHashTable *components,
                                         const char *account,
                                         const char *room){
  time_t mtime;
  struct tm * timeinfo;

  g_autofree gchar *iso_timestamp = g_malloc0(MAX_GMT_ISO_SIZE * sizeof(char));

  mtime = chatty_history_get_chat_last_message_time(account, room);
  mtime += 1; // Use the next epoch to exclude the last stored message(s)
  timeinfo = gmtime (&mtime);
  g_return_if_fail (strftime (iso_timestamp,
                              MAX_GMT_ISO_SIZE * sizeof(char),
                              "%Y-%m-%dT%H:%M:%SZ",
                              timeinfo));

  g_hash_table_steal (components, "history_since");
  g_hash_table_insert (components, "history_since", g_steal_pointer(&iso_timestamp));
}


/**
 * chatty_conv_join_chat:
 * @chat: a PurpleChat
 *
 * Joins a group chat
 * If there is already an instance of the chat
 * the GUI presents it to the user.
 *
 */
void
chatty_conv_join_chat (PurpleChat *chat)
{
  PurpleAccount            *account;
  PurpleConversation       *conv;
  PurplePluginProtocolInfo *prpl_info;
  GHashTable               *components;
  const char               *name;
  char                     *chat_name;

  ChattyConversation *chatty_conv;

  account = purple_chat_get_account(chat);
  prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_find_prpl (purple_account_get_protocol_id (account)));

  components = purple_chat_get_components (chat);

  if (prpl_info && prpl_info->get_chat_name) {
    chat_name = prpl_info->get_chat_name(components);
  } else {
    chat_name = NULL;
  }

  if (chat_name) {
    name = chat_name;
  } else {
    name = purple_chat_get_name(chat);
  }

  conv = purple_find_conversation_with_account (PURPLE_CONV_TYPE_CHAT,
                                                name,
                                                account);

  if (!conv || purple_conv_chat_has_left (PURPLE_CONV_CHAT(conv))) {
    chatty_conv_add_history_since_component(components, account->username, name);
    serv_join_chat (purple_account_get_connection (account), components);
  } else if (conv) {
    purple_conversation_present(conv);

    chatty_conv = CHATTY_CONVERSATION (conv);

    chatty_conv_set_unseen (chatty_conv, CHATTY_UNSEEN_NONE);
  }

  g_free (chat_name);
}


/**
 * chatty_conv_setup_pane:
 * @chatty_conv: A ChattyConversation
 *
 * This function is called from #chatty_conv_new to
 * set a pane for a chat. It includes the message-list,
 * the message-entry and the send button.
 *
 * Returns: a GtkBox container with the pane widgets
 *
 */
static GtkWidget *
chatty_conv_setup_pane (ChattyConversation *chatty_conv,
                        guint               msg_type)
{
  gtk_icon_theme_add_resource_path (gtk_icon_theme_get_default (),
                                    "/sm/puri/chatty/icons/ui/");

  chatty_conv->chat_view = chatty_chat_view_new ();
  chatty_chat_view_set_conv (CHATTY_CHAT_VIEW (chatty_conv->chat_view), chatty_conv);

  return chatty_conv->chat_view;
}


/**
 * chatty_conv_new:
 * @conv: a PurpleConversation
 *
 * This function is called via PurpleConversationUiOps
 * when conv is created (but before the
 * conversation-created signal is emitted).
 *
 */
void
chatty_conv_new (PurpleConversation *conv)
{
  PurpleAccount      *account;
  PurpleBuddy        *buddy;
  PurpleValue        *value;
  PurpleBlistNode    *conv_node;
  ChattyConversation *chatty_conv;
  const gchar        *protocol_id;
  const gchar        *conv_name;
  const gchar        *folks_name;
  guint               msg_type;

  PurpleConversationType conv_type = purple_conversation_get_type (conv);

  if (conv_type == PURPLE_CONV_TYPE_IM && (chatty_conv = chatty_conv_find_conv (conv))) {
    conv->ui_data = chatty_conv;
    return;
  }

  chatty_conv = g_new0 (ChattyConversation, 1);
  conv->ui_data = chatty_conv;
  chatty_conv->conv = conv;

  account = purple_conversation_get_account (conv);
  protocol_id = purple_account_get_protocol_id (account);

  if (conv_type == PURPLE_CONV_TYPE_CHAT) {

    msg_type = CHATTY_MSG_TYPE_MUC;
  } else if (conv_type == PURPLE_CONV_TYPE_IM) {
    // Add SMS and IMs from unknown contacts to the chats-list,
    // but do not add them to the contacts-list and in case of
    // instant messages do not sync contacts with the server
    conv_name = purple_conversation_get_name (conv);
    buddy = purple_find_buddy (account, conv_name);

    if (g_strcmp0 (protocol_id, "prpl-mm-sms") == 0) {
      if (buddy == NULL) {
        ChattyEds *chatty_eds;
        ChattyContact *contact;

        chatty_eds = chatty_manager_get_eds (chatty_manager_get_default ());
        contact = chatty_eds_find_by_number (chatty_eds, conv_name);

        if (contact) {
          folks_name = chatty_item_get_name (CHATTY_ITEM (contact));

          buddy = purple_buddy_new (account, conv_name, folks_name);

          purple_blist_add_buddy (buddy, NULL, NULL, NULL);
        }
      }

      msg_type = CHATTY_MSG_TYPE_SMS;
    } else {
      msg_type = CHATTY_MSG_TYPE_IM;
    }

    if (buddy == NULL) {
      buddy = purple_buddy_new (account, conv_name, NULL);
      purple_blist_add_buddy (buddy, NULL, NULL, NULL);
      // flag the node in the blist so it can be set off in the chats-list
      purple_blist_node_set_bool (PURPLE_BLIST_NODE(buddy), "chatty-unknown-contact", TRUE);

      g_debug ("Unknown contact %s added to blist", purple_buddy_get_name (buddy));
    }
  }

  chatty_conv->tab_cont = chatty_conv_setup_pane (chatty_conv, msg_type);
  g_object_set_data (G_OBJECT(chatty_conv->tab_cont),
                     "ChattyConversation",
                     chatty_conv);

  gtk_widget_show (chatty_conv->tab_cont);

  if (chatty_conv->tab_cont == NULL) {
    g_free (chatty_conv);
    conv->ui_data = NULL;
    return;
  }

  chatty_conv_stack_add_conv (chatty_conv);

  conv_node = chatty_get_conv_blist_node (conv);

  if (conv_node != NULL &&
      (value = g_hash_table_lookup (conv_node->settings, "enable-logging")) &&
      purple_value_get_type (value) == PURPLE_TYPE_BOOLEAN)
  {
    purple_conversation_set_logging (conv, purple_value_get_boolean (value));
  }

  chatty_chat_view_load (CHATTY_CHAT_VIEW (chatty_conv->chat_view), LAZY_LOAD_INITIAL_MSGS_LIMIT);
}


/**
 * chatty_conv_destroy:
 * @conv: a PurpleConversation
 *
 * This function is called via PurpleConversationUiOps
 * before a conversation is freed.
 *
 */
static void
chatty_conv_destroy (PurpleConversation *conv)
{
  ChattyConversation *chatty_conv;

  chatty_conv = CHATTY_CONVERSATION (conv);

  chatty_conv_remove_conv (chatty_conv);

  g_debug ("chatty_conv_destroy conv");

  g_free (chatty_conv);
}


void *
chatty_conversations_get_handle (void)
{
  static int handle;

  return &handle;
}


/**
 * PurpleConversationUiOps:
 *
 * The interface struct for libpurple conversation events.
 * Callbackhandler for the UI are assigned here.
 *
 */
static PurpleConversationUiOps conversation_ui_ops =
{
  chatty_conv_new,
  chatty_conv_destroy,
  chatty_conv_write_chat,
  chatty_conv_write_im,
  chatty_conv_write_conversation,
  chatty_conv_muc_list_add_users,
  NULL,
  NULL,
  chatty_conv_muc_list_update_user,
  chatty_conv_present_conversation,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};


PurpleConversationUiOps *
chatty_conversations_get_conv_ui_ops (void)
{
  return &conversation_ui_ops;
}


/**
 * chatty_init_conversations:
 *
 * Sets purple conversations preferenz values
 * and defines libpurple signal callbacks
 *
 */
void
chatty_conversations_init (void)
{
  void *handle = chatty_conversations_get_handle ();
  void *blist_handle = purple_blist_get_handle ();

  purple_prefs_add_none (CHATTY_PREFS_ROOT "/conversations");
  purple_prefs_add_bool (CHATTY_PREFS_ROOT "/conversations/show_tabs", FALSE);

  purple_signal_register (handle, "conversation-write",
                          purple_marshal_VOID__POINTER_POINTER_POINTER_UINT,
                          NULL, 4,
                          purple_value_new(PURPLE_TYPE_SUBTYPE,
                                  PURPLE_SUBTYPE_ACCOUNT),
                          purple_value_new (PURPLE_TYPE_BOXED,
                                  "PurpleConvMessage *"),
                          purple_value_new(PURPLE_TYPE_POINTER),
                          purple_value_new(PURPLE_TYPE_ENUM));

  /**
   * This is default fallback history handler which is called last,
   * other plugins may intercept and suppress it if they handle history
   * on their own (eg. MAM)
   */
  purple_signal_connect_priority(handle,
                                "conversation-write", handle,
                                PURPLE_CALLBACK (chatty_history_add_message),
                                NULL, PURPLE_SIGNAL_PRIORITY_HIGHEST);

  purple_signal_connect (blist_handle, "buddy-icon-changed",
                          handle, PURPLE_CALLBACK (cb_update_buddy_icon), NULL);

  purple_signal_connect (purple_conversations_get_handle (),
                         "buddy-typing", &handle,
                         PURPLE_CALLBACK (cb_buddy_typing), NULL);

  purple_signal_connect (purple_conversations_get_handle (),
                         "buddy-typed", &handle,
                         PURPLE_CALLBACK (cb_buddy_typed), NULL);

  purple_signal_connect (purple_conversations_get_handle (),
                         "buddy-typing-stopped", &handle,
                         PURPLE_CALLBACK (cb_buddy_typed), NULL);

  purple_cmd_register ("chatty",
                       "ww",
                       PURPLE_CMD_P_DEFAULT,
                       PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS,
                       NULL,
                       cb_chatty_cmd,
                       "chatty &lt;help&gt;:  "
                       "For a list of commands use the 'help' argument.",
                       NULL);

  chatty_chat_view_purple_init ();
}


void
chatty_conversations_uninit (void)
{
  chatty_chat_view_purple_uninit ();
  purple_prefs_disconnect_by_handle (chatty_conversations_get_handle());
  purple_signals_disconnect_by_handle (chatty_conversations_get_handle());
  purple_signals_unregister_by_instance (chatty_conversations_get_handle());
}
