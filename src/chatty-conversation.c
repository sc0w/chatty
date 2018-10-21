/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "purple.h"
#include <glib.h>
#include <glib/gi18n.h>
#include "chatty-window.h"
#include "chatty-purple-init.h"
#include "chatty-message-list.h"
#include "chatty-conversation.h"


#define MAX_MSGS 50


static void
chatty_conv_write_conversation (PurpleConversation *conv,
                                const char         *who,
                                const char         *alias,
                                const char         *message,
                                PurpleMessageFlags  flags,
                                time_t              mtime);


void chatty_conv_new (PurpleConversation *conv);
static gboolean chatty_conv_check_for_command (PurpleConversation *conv);
ChattyConversation * chatty_conv_container_get_active_chatty_conv (GtkNotebook *notebook);
PurpleConversation * chatty_conv_container_get_active_purple_conv (GtkNotebook *notebook);
void chatty_conv_switch_active_conversation (PurpleConversation *conv);


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

  if (chatty_conv && chatty_conv->active_conv == conv) {
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

  if (chatty_conv && chatty_conv->active_conv == conv) {
    chatty_msg_list_hide_typing_indicator (chatty_conv->msg_list);
  }
}


static void
cb_buddy_typing_stopped (PurpleAccount *account,
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

  if (chatty_conv && chatty_conv->active_conv == conv) {
    chatty_msg_list_hide_typing_indicator (chatty_conv->msg_list);
  }
}


static void
cb_update_buddy_status (PurpleBuddy  *buddy,
                        PurpleStatus *old,
                        PurpleStatus *newstatus)
{
  // TODO set status icon in the buddy info-popover
  // which can be launched from the headerbar
  // in the messages view
}


static void
cb_button_send_clicked (GtkButton *sender,
                        gpointer   data)
{
  PurpleConversation  *conv;
  ChattyConversation  *chatty_conv;
  PurpleAccount       *account;
  GtkTextIter          start, end;
  gchar               *message = NULL;

  chatty_conv  = (ChattyConversation *)data;
  conv = chatty_conv->active_conv;

  account = purple_conversation_get_account (conv);

  if (chatty_conv_check_for_command (conv)) {
    return;
  }

  if (!purple_account_is_connected (account)) {
    return;
  }

  gtk_widget_grab_focus (chatty_conv->msg_entry);

  purple_idle_touch ();

  if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)

  gtk_text_buffer_get_bounds (chatty_conv->msg_buffer, &start, &end);

  message = gtk_text_buffer_get_text (chatty_conv->msg_buffer,
                                      &start,
                                      &end,
                                      FALSE);

  if (gtk_text_buffer_get_char_count (chatty_conv->msg_buffer)) {
    purple_conv_im_send (PURPLE_CONV_IM (conv), message);
    gtk_text_buffer_delete (chatty_conv->msg_buffer,
                            &start,
                            &end);
  }

  g_free (message);
}


static gboolean
cb_textview_keypress (GtkWidget   *widget,
                      GdkEventKey *pKey,
                      gpointer     data)
{
  if (pKey->type == GDK_KEY_PRESS && pKey->keyval == GDK_KEY_Return)
    cb_button_send_clicked (NULL, data);

  return FALSE;
}


static void
cb_received_im_msg (PurpleAccount       *account,
                    const char          *sender,
                    const char          *message,
                    PurpleConversation  *conv,
                    PurpleMessageFlags   flags)
{
  guint timer;

  if (conv) {
    timer = GPOINTER_TO_INT(purple_conversation_get_data (conv, "close-timer"));
    if (timer) {
      purple_timeout_remove (timer);
      purple_conversation_set_data (conv, "close-timer", GINT_TO_POINTER(0));
    }
  }
}


static void
cb_conversation_switched (PurpleConversation *conv)
{
  // update conversation headerbar
  // with avatar and status icon etc.
}


static ChattyConversation *
chatty_conv_get_conv_at_index (GtkNotebook *notebook,
                               int          index)
{
  GtkWidget *tab_cont;

  if (index == -1) {
    index = 0;
  }

  tab_cont = gtk_notebook_get_nth_page (GTK_NOTEBOOK(notebook), index);

  return tab_cont ?
    g_object_get_data (G_OBJECT(tab_cont), "ChattyConversation") : NULL;
}


static void
cb_stack_cont_before_switch_conv (GtkNotebook *notebook,
                                  GtkWidget   *page,
                                  gint         page_num,
                                  gpointer     user_data)
{
  PurpleConversation *conv;
  ChattyConversation *chatty_conv;

  conv = chatty_conv_container_get_active_purple_conv (notebook);

  g_return_if_fail(conv != NULL);

  if (purple_conversation_get_type(conv) != PURPLE_CONV_TYPE_IM) {
    return;
  }

  chatty_conv = CHATTY_CONVERSATION(conv);

  chatty_msg_list_hide_typing_indicator (chatty_conv->msg_list);
}


static void
cb_stack_cont_switch_conv (GtkNotebook *notebook,
                           GtkWidget   *page,
                           gint         page_num,
                           gpointer     user_data)
{
  PurpleConversation *conv;
  ChattyConversation *chatty_conv;

  chatty_conv = chatty_conv_get_conv_at_index (GTK_NOTEBOOK(notebook), page_num);

  conv = chatty_conv->active_conv;

  g_return_if_fail (conv != NULL);

  chatty_conv_switch_active_conversation (conv);

  chatty_conv = CHATTY_CONVERSATION(conv);

  chatty_conv_set_unseen (chatty_conv, CHATTY_UNSEEN_NONE);
}


// *** end callbacks


// TODO
// Needs to be set up for command handling in the message view
// Mainly for testing purposes (SMS + OMEMO plugin)
// Commands have to be registered with the libpurple core
// accordingly then (like for key ackn in OMEMO or SMS operations)
static gboolean
chatty_conv_check_for_command (PurpleConversation *conv)
{
  ChattyConversation *chatty_conv;
  gchar              *cmd;
  const gchar        *prefix;
  gboolean            retval = FALSE;
  GtkTextIter         start, end;

  chatty_conv = CHATTY_CONVERSATION(conv);

  prefix = "/";

  gtk_text_buffer_get_bounds (chatty_conv->msg_buffer,
                              &start,
                              &end);

  cmd = gtk_text_buffer_get_text (chatty_conv->msg_buffer,
                                  &start, &end,
                                  FALSE);

  if (cmd && (strncmp (cmd, prefix, strlen (prefix)) == 0)) {
    PurpleCmdStatus status;
    gchar *error, *cmdline;

    cmdline = cmd + strlen (prefix);

    if (purple_strequal (cmdline, "xyzzy")) {
      purple_conversation_write (conv, "", "Nothing happens",
          PURPLE_MESSAGE_NO_LOG, time(NULL));

      g_free (cmd);
      return TRUE;
    }

    status = purple_cmd_do_command (conv, cmdline, NULL, &error);

    switch (status) {
      case PURPLE_CMD_STATUS_OK:
        retval = TRUE;
        break;
      case PURPLE_CMD_STATUS_NOT_FOUND:
      {
        PurplePluginProtocolInfo *prpl_info = NULL;
        PurpleConnection *gc;

        if ((gc = purple_conversation_get_gc (conv)))
          prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

        if ((prpl_info != NULL) && (prpl_info->options &
            OPT_PROTO_SLASH_COMMANDS_NATIVE)) {
          gchar *spaceslash;

          /* If the first word in the entered text has a '/' in it, then the user
           * probably didn't mean it as a command. So send the text as message. */
          spaceslash = cmdline;

          while (*spaceslash && *spaceslash != ' ' && *spaceslash != '/') {
            spaceslash++;
          }

          if (*spaceslash != '/') {
            purple_conversation_write (conv,
                                       "",
                                       _("Unknown command."),
                                       PURPLE_MESSAGE_NO_LOG,
                                       time(NULL));
            retval = TRUE;
          }
        }
        break;
      }
      case PURPLE_CMD_STATUS_WRONG_ARGS:
        purple_conversation_write (conv,
                                   "",
                                   _("Wrong number of arguments for the command."),
                                   PURPLE_MESSAGE_NO_LOG,
                                   time(NULL));
        retval = TRUE;
        break;
      case PURPLE_CMD_STATUS_FAILED:
        purple_conversation_write (conv,
                                   "",
                                   error ? error : _("The command failed."),
                                   PURPLE_MESSAGE_NO_LOG,
                                   time(NULL));
        g_free(error);
        retval = TRUE;
        break;
      case PURPLE_CMD_STATUS_WRONG_TYPE:
        if (purple_conversation_get_type (conv) == PURPLE_CONV_TYPE_IM)
          purple_conversation_write (conv,
                                     "",
                                     _("That command only works in chats, not IMs."),
                                     PURPLE_MESSAGE_NO_LOG,
                                     time(NULL));
        else
          purple_conversation_write (conv,
                                     "",
                                     _("That command only works in IMs, not chats."),
                                     PURPLE_MESSAGE_NO_LOG,
                                     time(NULL));
        retval = TRUE;
        break;
      case PURPLE_CMD_STATUS_WRONG_PRPL:
        purple_conversation_write (conv,
                                   "",
                                   _("That command doesn't work on this protocol."),
                                   PURPLE_MESSAGE_NO_LOG,
                                   time(NULL));
        retval = TRUE;
        break;
      default:
        break; /* nothing to do */
    }
  }

  g_free (cmd);
  return retval;
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

  purple_conversation_set_data (chatty_conv->active_conv, "unseen-count",
                                GINT_TO_POINTER(chatty_conv->unseen_count));

  purple_conversation_set_data (chatty_conv->active_conv, "unseen-state",
                                GINT_TO_POINTER(chatty_conv->unseen_state));

  purple_conversation_update (chatty_conv->active_conv, PURPLE_CONV_UPDATE_UNSEEN);
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

    if(chatty_conv == NULL || chatty_conv->active_conv != conv) {
      continue;
    }

    if (chatty_conv->unseen_state == state) {
      r = g_list_prepend(r, conv);
      c++;
    }
  }

  return r;
}


static ChattyLog*
parse_message (const gchar* msg)
{
  ChattyLog *log;
  g_auto(GStrv) timesplit=NULL, accountsplit=NULL, namesplit=NULL;

  if (msg == NULL)
    return NULL;

  /* Separate the timestamp from the rest of the message */
  timesplit = g_strsplit (msg, ") ", -1);
  /* Format is '(x:y:z' */
  if (timesplit[0] == NULL || strlen (timesplit[0]) < 6)
    return NULL;

  log = g_new0 (ChattyLog, 1);
  log->time_stamp = g_strdup(&(timesplit[0][1]));

  if (timesplit[1] == NULL)
    return log;

  accountsplit = g_strsplit (timesplit[1], ": ", -1);

  if (accountsplit[0] == NULL)
    return log;

  namesplit = g_strsplit (accountsplit[0], "/", -1);
  log->name = g_strdup (namesplit[0]);

  if (accountsplit[1] == NULL)
    return log;

  log->msg = g_strdup (accountsplit[1]);
  return log;
}

/**
 * chatty_add_message_history_to_conv:
 * @data: a ChattyConversation
 *
 * Parse the chat log and add the
 * messages to a message-list
 *
 */
static gboolean
chatty_add_message_history_to_conv (gpointer data)
{
  ChattyConversation *chatty_conv = data;

  // TODO
  // this parser for the purple log files is
  // just a tentative solution.
  // Parsing the purple logfile data is cumbersome, and
  // Chatty needs special log functionality for the
  // send reports anyway. Also for infinite scrolling
  // and for just pulling particular data, the logging
  // should be built around a sqlLite database.

  int timer = chatty_conv->attach.timer;

  gboolean im = (chatty_conv->active_conv->type == PURPLE_CONV_TYPE_IM);

  chatty_conv->attach.timer = 0;
  chatty_conv->attach.timer = timer;

  if (chatty_conv->attach.current) {
    return TRUE;
  }

  g_source_remove (chatty_conv->attach.timer);
  chatty_conv->attach.timer = 0;

  if (im) {
    GList         *msgs = NULL;
    ChattyLog     *log_data = NULL;
    PurpleAccount *account;
    const gchar   *name = NULL;
    const gchar   *conv_name;
    gchar         *read_log;
    gchar         *stripped;
    gchar         **line_split = NULL;
    gchar         **logs = NULL;
    gchar         *time_stamp;
    GList         *history;

    conv_name = purple_conversation_get_name (chatty_conv->active_conv);
    account = purple_conversation_get_account (chatty_conv->active_conv);
    name = purple_buddy_get_name (purple_find_buddy (account, conv_name));
    history = purple_log_get_logs (PURPLE_LOG_IM, name, account);

    if (history == NULL) {
      g_list_free (history);
      return FALSE;
    }

    name = purple_buddy_get_alias (purple_find_buddy (account, conv_name));

    // limit the log-list to MAX_MSGS msgs since we currently have no
    // infinite scrolling implemented
    for (int i = 0; history && i < MAX_MSGS; history = history->next) {
      read_log = purple_log_read ((PurpleLog*)history->data, NULL);
      stripped = purple_markup_strip_html (read_log);

      logs = g_strsplit (stripped, "\n", -1);

      for (int num = 0; num < (g_strv_length (logs) - 1); num++) {
	log_data = parse_message (logs[num]);
	if (log_data) {
	  i++;
	  msgs = g_list_prepend (msgs, (gpointer)log_data);
	}
      }

      line_split = g_strsplit (name, "/", -1);
      name = g_strdup (line_split[0]);

      g_free (read_log);
      g_free (stripped);
      g_strfreev (logs);
      g_strfreev (line_split);
    }

    g_list_foreach (history, (GFunc)purple_log_free, NULL);
    g_list_free (history);

    for (; msgs; msgs = msgs->next) {
      log_data = msgs->data;

      if (msgs == (g_list_last (msgs))) {
        time_stamp = log_data->time_stamp;
      } else {
        time_stamp = NULL;
      }

      if ((g_strcmp0 (log_data->name, name)) == 0) {
        chatty_msg_list_add_message (chatty_conv->msg_list,
                                     MSG_IS_INCOMING,
                                     log_data->msg,
                                     time_stamp);
      } else {
        chatty_msg_list_add_message (chatty_conv->msg_list,
                                     MSG_IS_OUTGOING,
                                     log_data->msg,
                                     time_stamp);
      }
    }

    g_list_foreach (msgs, (GFunc)g_free, NULL);
    g_list_free (msgs);

    g_object_set_data (G_OBJECT (chatty_conv->msg_entry),
                       "attach-start-time",
                       NULL);
  }

  g_object_set_data (G_OBJECT (chatty_conv->msg_entry),
                     "attach-start-time",
                     NULL);

  return FALSE;
}


/**
 * chatty_conv_container_get_active_chatty_conv:
 * @notebook: a GtkNotebook
 *
 * Returns the purple conversation that is
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
 * chatty_conv_container_get_active_purple_conv:
 * @notebook: a GtkNotebook
 *
 * Returns the purple conversation that is
 * currently set active in the notebook
 *
 * Returns: PurpleConversation
 *
 */
PurpleConversation *
chatty_conv_container_get_active_purple_conv (GtkNotebook *notebook)
{
  ChattyConversation *chatty_conv;

  chatty_conv = chatty_conv_container_get_active_chatty_conv (notebook);

  return chatty_conv ? chatty_conv->active_conv : NULL;
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
  PurpleConversation      *conv = chatty_conv->active_conv;
  const gchar             *tab_txt;
  gchar                   *text;
  gchar                   **name_split;

  chatty_data_t *chatty = chatty_get_data();

  tab_txt = purple_conversation_get_title (conv);

  gtk_notebook_append_page (GTK_NOTEBOOK(chatty->pane_view_message_list),
                            chatty_conv->tab_cont, NULL);

  name_split = g_strsplit (tab_txt, "@", -1);
  text = g_strdup_printf("%s %s",name_split[0], " >");

  gtk_notebook_set_tab_label_text (GTK_NOTEBOOK(chatty->pane_view_message_list),
                                   chatty_conv->tab_cont, text);

  gtk_widget_show (chatty_conv->tab_cont);

  if (g_list_length(chatty_conv->convs) == 1) {
    gtk_notebook_set_current_page (GTK_NOTEBOOK(chatty->pane_view_message_list), 0);
  } else {
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK(chatty->pane_view_message_list), FALSE);
  }

  g_free (text);
  g_strfreev (name_split);

  gtk_widget_grab_focus (chatty_conv->msg_entry);
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

  if (conv != chatty_conv->active_conv &&
      flags & PURPLE_MESSAGE_ACTIVE_ONLY)
  {
    return;
  }

  purple_conversation_write (conv, who, message, flags, mtime);
}


/**
 * chatty_conv_write_common:
 * @conv:     a PurpleConversation
 * @who:      the buddy name
 * @message:  the message text
 * @flags:    PurpleMessageFlags
 * @mtime:    mtime
 *
 * Writes a message to the message list
 *
 */
static void
chatty_conv_write_common (PurpleConversation *conv,
                          const char         *who,
                          const char         *message,
                          PurpleMessageFlags  flags,
                          time_t              mtime)
{
  ChattyConversation     *chatty_conv;
  PurpleConnection       *gc;
  PurpleAccount          *account;
  gchar                  *strip, *newline;

  chatty_conv = CHATTY_CONVERSATION (conv);

  if (chatty_conv->attach.timer) {
    /* We are currently in the process of filling up the buffer with the message
     * history of the conversation. So we do not need to add the message here.
     * Instead, this message will be added to the message-list, which in turn will
     * be processed and displayed by the attach-callback.
     */
    return;
  }

  g_return_if_fail (chatty_conv != NULL);

  if ((flags & PURPLE_MESSAGE_SYSTEM) && !(flags & PURPLE_MESSAGE_NOTIFY)) {
    flags &= ~(PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV);
  }

  account = purple_conversation_get_account(conv);
  g_return_if_fail(account != NULL);
  gc = purple_account_get_connection(account);
  g_return_if_fail(gc != NULL || !(flags & (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV)));

  if (flags & PURPLE_MESSAGE_SEND) {
    newline = purple_strdup_withhtml (message);
    strip = purple_markup_strip_html (newline);

    chatty_msg_list_add_message (chatty_conv->msg_list,
                                 MSG_IS_OUTGOING,
                                 strip,
                                 NULL);

    chatty_conv_set_unseen (chatty_conv, CHATTY_UNSEEN_NONE);

    g_free (newline);
    g_free (strip);
  }

  if (flags & PURPLE_MESSAGE_RECV) {
    newline = purple_strdup_withhtml (message);
    strip = purple_markup_strip_html (newline);

    chatty_msg_list_add_message (chatty_conv->msg_list,
                                 MSG_IS_INCOMING,
                                 strip,
                                 NULL);

    chatty_conv_set_unseen (chatty_conv, CHATTY_UNSEEN_TEXT);

    g_free (newline);
    g_free (strip);
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
  const gchar *name;

  if (alias && *alias) {
    name = alias;
  } else if (who && *who) {
    name = who;
  } else {
    name = NULL;
  }

  chatty_conv_write_common (conv, name, message, flags, mtime);
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
      node = node ? node->parent : NULL;
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
 * chatty_conv_switch_active_conversation:
 * @conv: a PurpleConversation
 *
 * Makes the PurpleConversation #conv the active
 * #chatty_conv conversation and sets the GUI
 * accordingly.
 *
 */
void
chatty_conv_switch_active_conversation (PurpleConversation *conv)
{
  ChattyConversation *chatty_conv;
  PurpleConversation *old_conv;

  g_return_if_fail (conv != NULL);

  chatty_conv = CHATTY_CONVERSATION (conv);
  old_conv = chatty_conv->active_conv;

  if (old_conv == conv) {
    return;
  }

  purple_conversation_close_logs (old_conv);

  chatty_conv->active_conv = conv;

  purple_conversation_set_logging (conv, TRUE);

  purple_signal_emit (chatty_conversations_get_handle (),
                      "conversation-switched",
                      conv);

 // TODO set headerbar  (same with icons for online stat/avatar)
}



/**
 * chatty_conv_switch_conv:
 * @chatty_conv: a ChattyConversation
 *
 * Brings the conversation-pane of chatty_conv to
 * the front
 *
 * Returns: return value
 *
 */
static void
chatty_conv_switch_conv (ChattyConversation *chatty_conv)
{
  chatty_data_t *chatty = chatty_get_data();

  gtk_notebook_set_current_page (GTK_NOTEBOOK(chatty->pane_view_message_list),
                                 gtk_notebook_page_num (GTK_NOTEBOOK(chatty->pane_view_message_list),
                                 chatty_conv->tab_cont));
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

  chatty_conv_switch_active_conversation (conv);

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
  ChattyConversation *chatty_conv;

  g_return_if_fail (account != NULL);
  g_return_if_fail (name != NULL);

  conv = purple_find_conversation_with_account (PURPLE_CONV_TYPE_IM,
                                                name,
                                                account);

  if (conv == NULL)
    conv = purple_conversation_new (PURPLE_CONV_TYPE_IM,
                                    account,
                                    name);

  purple_signal_emit (chatty_conversations_get_handle (),
                      "conversation-displayed",
                      CHATTY_CONVERSATION (conv));

  chatty_conv_present_conversation (conv);

  chatty_conv = CHATTY_CONVERSATION (conv);

  chatty_conv_set_unseen (chatty_conv, CHATTY_UNSEEN_NONE);
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
chatty_conv_setup_pane (ChattyConversation *chatty_conv)
{
  GtkWidget       *vbox;
  GtkWidget       *hbox;
  GtkWidget       *scrolled_window;
  GtkWidget       *button_send;
  GtkImage        *image;
  GtkStyleContext *sc;

  PurpleConversation *conv = chatty_conv->active_conv;

  chatty_conv->msg_buffer = gtk_text_buffer_new (NULL);
  chatty_conv->msg_entry = gtk_text_view_new_with_buffer (chatty_conv->msg_buffer);
  g_object_set_data(G_OBJECT(chatty_conv->msg_buffer), "user_data", chatty_conv);
  sc = gtk_widget_get_style_context (chatty_conv->msg_entry);
  gtk_style_context_add_class (sc, "text_view");
  g_signal_connect (G_OBJECT(chatty_conv->msg_entry),
                    "key_press_event",
                    G_CALLBACK(cb_textview_keypress),
                    (gpointer) chatty_conv);

  gtk_text_view_set_left_margin (GTK_TEXT_VIEW (chatty_conv->msg_entry), 10);
  gtk_text_view_set_top_margin (GTK_TEXT_VIEW (chatty_conv->msg_entry), 10);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (chatty_conv->msg_entry), GTK_WRAP_WORD);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  sc = gtk_widget_get_style_context (scrolled_window);
  gtk_style_context_add_class (sc, "text_view_scroll");
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  gtk_container_add (GTK_CONTAINER (scrolled_window), chatty_conv->msg_entry);
  gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 5);
  gtk_box_pack_start (GTK_BOX (hbox), scrolled_window, TRUE, TRUE, 5);

  button_send = gtk_button_new ();
  gtk_widget_set_size_request (button_send, 45, 45);
  sc = gtk_widget_get_style_context (button_send);

  switch (purple_conversation_get_type (conv)) {
    case PURPLE_CONV_TYPE_UNKNOWN:
      gtk_style_context_add_class (sc, "button_send_green");
      break;
    case PURPLE_CONV_TYPE_IM:
      gtk_style_context_add_class (sc, "button_send_blue");
      break;
    case PURPLE_CONV_TYPE_CHAT:
    case PURPLE_CONV_TYPE_MISC:
    case PURPLE_CONV_TYPE_ANY:
    default:
      break;
  }

  g_signal_connect (button_send, "clicked",
                    G_CALLBACK(cb_button_send_clicked),
                    (gpointer) chatty_conv);

  image = GTK_IMAGE (gtk_image_new_from_icon_name ("pan-up-symbolic",
						   GTK_ICON_SIZE_BUTTON));

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_button_set_image (GTK_BUTTON (button_send), GTK_WIDGET (image));
G_GNUC_END_IGNORE_DEPRECATIONS
  gtk_widget_set_valign (button_send, GTK_ALIGN_CENTER);

  gtk_box_pack_start (GTK_BOX (hbox), button_send, FALSE, FALSE, 6);

  chatty_conv->msg_list = CHATTY_MSG_LIST (chatty_msg_list_new (MSG_TYPE_IM, TRUE));

  gtk_box_pack_start (GTK_BOX (vbox),
                      GTK_WIDGET (chatty_conv->msg_list),
                      TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox),
                      hbox, FALSE, FALSE, 0);

  gtk_widget_show_all (vbox);

  return vbox;
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
  PurpleValue        *value;
  PurpleBlistNode    *conv_node;
  ChattyConversation *chatty_conv;

  PurpleConversationType conv_type = purple_conversation_get_type (conv);

  if (conv_type == PURPLE_CONV_TYPE_IM && (chatty_conv = chatty_conv_find_conv (conv))) {
    conv->ui_data = chatty_conv;
    if (!g_list_find (chatty_conv->convs, conv))
      chatty_conv->convs = g_list_prepend (chatty_conv->convs, conv);

    chatty_conv_switch_active_conversation (conv);

    return;
  }

  chatty_conv = g_new0 (ChattyConversation, 1);
  conv->ui_data = chatty_conv;
  chatty_conv->active_conv = conv;
  chatty_conv->convs = g_list_prepend (chatty_conv->convs, conv);

  if (conv_type == PURPLE_CONV_TYPE_IM) {
    chatty_conv->conv_header = g_malloc0 (sizeof (ChattyConvViewHeader));
  }

  chatty_conv->tab_cont = chatty_conv_setup_pane (chatty_conv);
  g_object_set_data (G_OBJECT(chatty_conv->tab_cont),
                     "ChattyConversation",
                     chatty_conv);
  gtk_widget_show (chatty_conv->tab_cont);

  if (chatty_conv->tab_cont == NULL) {
    if (conv_type == PURPLE_CONV_TYPE_IM) {
      g_free (chatty_conv->conv_header);
    }

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

  chatty_conv->attach.timer = g_idle_add (chatty_add_message_history_to_conv, chatty_conv);

  if (CHATTY_IS_CHATTY_CONVERSATION (conv)) {
    purple_signal_emit (chatty_conversations_get_handle (),
                        "conversation-displayed",
                        CHATTY_CONVERSATION (conv));
  }
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

  chatty_conv->convs = g_list_remove (chatty_conv->convs, conv);

  if (chatty_conv->convs) {
    if (chatty_conv->active_conv == conv) {
      chatty_conv->active_conv = chatty_conv->convs->data;
      purple_conversation_update (chatty_conv->active_conv,
                                  PURPLE_CONV_UPDATE_FEATURES);
    }

    return;
  }

  if (chatty_conv->attach.timer) {
    g_source_remove(chatty_conv->attach.timer);
  }

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
  NULL,
  chatty_conv_write_im,
  chatty_conv_write_conversation,
  NULL,
  NULL,
  NULL,
  NULL,
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
 * chatty_conv_container_init:
 *
 * Sets the notebook container for the
 * conversation panes
 *
 */
static void
chatty_conv_container_init (void)
{
  chatty_data_t *chatty = chatty_get_data();

  g_signal_connect (G_OBJECT(chatty->pane_view_message_list), "switch_page",
                    G_CALLBACK(cb_stack_cont_before_switch_conv), 0);
  g_signal_connect_after (G_OBJECT(chatty->pane_view_message_list), "switch_page",
                          G_CALLBACK(cb_stack_cont_switch_conv), 0);


  gtk_notebook_set_show_tabs (GTK_NOTEBOOK(chatty->pane_view_message_list), FALSE);
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

  chatty_conv_container_init ();

  purple_prefs_add_none (CHATTY_PREFS_ROOT "/conversations");
  purple_prefs_add_bool (CHATTY_PREFS_ROOT "/conversations/use_smooth_scrolling", TRUE);
  purple_prefs_add_bool (CHATTY_PREFS_ROOT "/conversations/im/show_buddy_icons", TRUE);

  purple_prefs_add_bool (CHATTY_PREFS_ROOT "/conversations/show_timestamps", TRUE);
  purple_prefs_add_bool (CHATTY_PREFS_ROOT "/conversations/tabs", TRUE);
  purple_prefs_add_int (CHATTY_PREFS_ROOT "/conversations/scrollback_lines", 4000);

  purple_signal_register (handle, "conversation-switched",
                          purple_marshal_VOID__POINTER, NULL, 1,
                          purple_value_new (PURPLE_TYPE_SUBTYPE,
                          PURPLE_SUBTYPE_CONVERSATION));

  purple_signal_register (handle, "conversation-hiding",
                          purple_marshal_VOID__POINTER, NULL, 1,
                          purple_value_new (PURPLE_TYPE_BOXED,
                          "ChattyConversation *"));

  purple_signal_register (handle, "conversation-displayed",
                          purple_marshal_VOID__POINTER, NULL, 1,
                          purple_value_new (PURPLE_TYPE_BOXED,
                          "ChattyConversation *"));

  purple_signal_connect (blist_handle, "buddy-status-changed",
                         handle, PURPLE_CALLBACK (cb_update_buddy_status), NULL);

  purple_signal_connect (purple_conversations_get_handle (),
                         "buddy-typing", &handle,
                         PURPLE_CALLBACK (cb_buddy_typing), NULL);

  purple_signal_connect (purple_conversations_get_handle (),
                         "buddy-typed", &handle,
                         PURPLE_CALLBACK (cb_buddy_typed), NULL);

  purple_signal_connect (purple_conversations_get_handle (),
                         "buddy-typing-stopped", &handle,
                         PURPLE_CALLBACK (cb_buddy_typing_stopped), NULL);

  purple_signal_connect (purple_conversations_get_handle (),
                         "received-im-msg",
                         &handle, PURPLE_CALLBACK (cb_received_im_msg), NULL);

  purple_signal_connect (chatty_conversations_get_handle(),
                         "conversation-switched",
                         handle, PURPLE_CALLBACK (cb_conversation_switched), NULL);
}


void
chatty_conversations_uninit (void)
{
  purple_prefs_disconnect_by_handle (chatty_conversations_get_handle());
  purple_signals_disconnect_by_handle (chatty_conversations_get_handle());
  purple_signals_unregister_by_instance (chatty_conversations_get_handle());
}
