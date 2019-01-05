/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#define G_LOG_DOMAIN "chatty-conversation"

#include "purple.h"
#include <glib.h>
#include <glib/gi18n.h>
#include "chatty-window.h"
#include "chatty-purple-init.h"
#include "chatty-message-list.h"
#include "chatty-conversation.h"


#define MAX_MSGS 50

static GHashTable *ht_sms_id = NULL;
static GHashTable *ht_emoticon = NULL;

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
static void chatty_update_typing_status (ChattyConversation *chatty_conv);
static void chatty_check_for_emoticon (ChattyConversation *chatty_conv);


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
cb_msg_list_message_added (ChattyMsgList *sender,
                           GtkWidget     *bubble,
                           gpointer       data)
{
  GtkWidget           *child;
  GList               *children;
  ChattyConversation  *chatty_conv;

  chatty_conv  = (ChattyConversation *)data;

  children = gtk_container_get_children (GTK_CONTAINER(bubble));

  do {
    child = children->data;

    if (g_strcmp0 (gtk_widget_get_name (child), "label-footer") == 0) {
      chatty_conv->msg_bubble_footer = child;
    }
  } while ((children = g_list_next (children)) != NULL);

  children = g_list_first (children);
  g_list_foreach (children, (GFunc)g_free, NULL);
  g_list_free (children);
}


static void
cb_sms_show_send_receipt (const char *sms_id,
                          int         status)
{
  GtkWidget   *bubble_footer;
  GDateTime   *time;
  gchar       *footer_str = NULL;
  const gchar *color;
  const gchar *symbol;

  if (sms_id == NULL) {
    return;
  }

  switch (status) {
    case CHATTY_SMS_RECEIPT_NONE:
      color = "<span color='red'>";
      symbol = " x";
      break;
    case CHATTY_SMS_RECEIPT_MM_ACKN:
      color = "<span color='grey'>";
      symbol = " ✓";
      break;
    case CHATTY_SMS_RECEIPT_SMSC_ACKN:
      color = "<span color='#6cba3d'>";
      symbol = " ✓";
      break;
    default:
      return;
  }

  time = g_date_time_new_now_local ();
  footer_str = g_date_time_format (time, "%R");
  g_date_time_unref (time);

  bubble_footer = (GtkWidget*) g_hash_table_lookup (ht_sms_id, sms_id);

  footer_str = g_strconcat ("<small>",
                            footer_str,
                            color,
                            symbol,
                            "</span></small>",
                            NULL);

  if (bubble_footer != NULL) {
    gtk_label_set_markup (GTK_LABEL(bubble_footer), footer_str);

    g_hash_table_remove (ht_sms_id, sms_id);
  }

  g_free (footer_str);
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
  const gchar         *protocol_id;
  gchar               *footer_str = NULL;
  gchar                sms_id_str[12];
  guint                sms_id;

  chatty_conv  = (ChattyConversation *)data;
  conv = chatty_conv->active_conv;

  account = purple_conversation_get_account (conv);

  gtk_text_buffer_get_bounds (chatty_conv->msg_buffer, &start, &end);

  if (chatty_conv_check_for_command (conv)) {
    gtk_widget_hide (chatty_conv->button_send);
    gtk_text_buffer_delete (chatty_conv->msg_buffer, &start, &end);
    return;
  }

  if (!purple_account_is_connected (account)) {
    return;
  }

  protocol_id = purple_account_get_protocol_id (account);

  gtk_widget_grab_focus (chatty_conv->msg_entry);

  purple_idle_touch ();

  message = gtk_text_buffer_get_text (chatty_conv->msg_buffer,
                                      &start,
                                      &end,
                                      FALSE);

  footer_str = g_strconcat ("<small>",
                            "<span color='grey'>",
                            " ✓",
                            "</span></small>",
                            NULL);

  if (gtk_text_buffer_get_char_count (chatty_conv->msg_buffer)) {
    chatty_msg_list_add_message (chatty_conv->msg_list,
                                 MSG_IS_OUTGOING,
                                 message,
                                 footer_str);

    // provide a msg-id to the sms-plugin for send-receipts
    if (g_strcmp0 (protocol_id, "prpl-mm-sms") == 0) {
      sms_id = g_random_int ();

      sprintf (sms_id_str, "%i", sms_id);

      g_hash_table_insert (ht_sms_id,
                           strdup (sms_id_str), chatty_conv->msg_bubble_footer);

      g_debug ("hash table insert sms_id_str: %s  ht_size: %i\n",
               sms_id_str, g_hash_table_size (ht_sms_id));

      purple_conv_im_send_with_flags (PURPLE_CONV_IM (conv),
                                      sms_id_str,
                                      PURPLE_MESSAGE_NO_LOG |
                                      PURPLE_MESSAGE_NOTIFY |
                                      PURPLE_MESSAGE_INVISIBLE);
    }

    purple_conv_im_send (PURPLE_CONV_IM (conv), message);

    gtk_widget_hide (chatty_conv->button_send);
  }

  gtk_text_buffer_delete (chatty_conv->msg_buffer, &start, &end);

  g_free (message);
  g_free (footer_str);
}


static gboolean
cb_textview_focus_in (GtkWidget *widget,
                      GdkEvent  *event,
                      gpointer   user_data)
{
  GtkStyleContext *sc;

  sc = gtk_widget_get_style_context (GTK_WIDGET(user_data));

  gtk_style_context_remove_class (sc, "msg_entry_defocused");
  gtk_style_context_add_class (sc, "msg_entry_focused");

  return FALSE;
}


static gboolean
cb_textview_focus_out (GtkWidget *widget,
                       GdkEvent  *event,
                       gpointer   user_data)
{
  GtkStyleContext *sc;

  sc = gtk_widget_get_style_context (GTK_WIDGET(user_data));

  gtk_style_context_remove_class (sc, "msg_entry_focused");
  gtk_style_context_add_class (sc, "msg_entry_defocused");

  return FALSE;
}


static gboolean
cb_textview_key_pressed (GtkWidget   *widget,
                         GdkEventKey *key_event,
                         gpointer     data)
{
  if (!purple_prefs_get_bool (CHATTY_PREFS_ROOT "/conversations/return_sends")) {
    return FALSE;
  }

  if (!key_event->state & GDK_SHIFT_MASK && key_event->keyval == GDK_KEY_Return) {
    cb_button_send_clicked (NULL, data);

    return TRUE;
  }

  return FALSE;
}


static gboolean
cb_textview_key_released (GtkWidget   *widget,
                          GdkEventKey *key_event,
                          gpointer     data)
{
  ChattyConversation  *chatty_conv;

  chatty_conv = (ChattyConversation *)data;

  if (gtk_text_buffer_get_char_count (chatty_conv->msg_buffer)) {
    gtk_widget_show (chatty_conv->button_send);
  } else {
    gtk_widget_hide (chatty_conv->button_send);
  }

  if (purple_prefs_get_bool (CHATTY_PREFS_ROOT "/conversations/send_typing")) {
    chatty_update_typing_status (chatty_conv);
  }

  if (purple_prefs_get_bool (CHATTY_PREFS_ROOT "/conversations/convert_emoticons")) {
    chatty_check_for_emoticon (chatty_conv);
  }

  return TRUE;
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

  g_return_if_fail (conv != NULL);

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


static void
cb_msg_input_vadjust (GObject     *sender,
                      GParamSpec  *pspec,
                      gpointer     data)
{
  GtkAdjustment *vadjust;
  GtkWidget     *vscroll;
  gdouble        upper;
  gdouble        page_size;
  gint           max_height;

  ChattyConversation *chatty_conv = data;

  vadjust = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW(chatty_conv->msg_scrolled));
  vscroll = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW(chatty_conv->msg_scrolled));
  upper = gtk_adjustment_get_upper (GTK_ADJUSTMENT(vadjust));
  page_size = gtk_adjustment_get_page_size (GTK_ADJUSTMENT(vadjust));
  max_height = gtk_scrolled_window_get_max_content_height (GTK_SCROLLED_WINDOW(chatty_conv->msg_scrolled));

  gtk_adjustment_set_value (vadjust, upper - page_size);

  if (upper > (gdouble)max_height) {
    gtk_widget_set_visible (vscroll, TRUE);
  } else {
    gtk_widget_set_visible (vscroll, FALSE);
  }

  gtk_widget_queue_draw (chatty_conv->msg_frame);
}


// *** end callbacks


static void
chatty_conv_init_emoticon_translations (void)
{
  ht_emoticon = g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       g_free,
                                       g_free);

  g_hash_table_insert (ht_emoticon, ":)", "🙂");
  g_hash_table_insert (ht_emoticon, ";)", "😉");
  g_hash_table_insert (ht_emoticon, ":(", "🙁");
  g_hash_table_insert (ht_emoticon, ":'(", "😢");
  g_hash_table_insert (ht_emoticon, ":/", "😕");
  g_hash_table_insert (ht_emoticon, ":D", "😀");
  g_hash_table_insert (ht_emoticon, ":'D", "😂");
  g_hash_table_insert (ht_emoticon, ";P", "😜");
  g_hash_table_insert (ht_emoticon, ":P", "😛");
  g_hash_table_insert (ht_emoticon, ":o", "😮");
  g_hash_table_insert (ht_emoticon, "B)", "😎 ");
  g_hash_table_insert (ht_emoticon, "SANTA", "🎅");
  g_hash_table_insert (ht_emoticon, "FROSTY", "⛄");
}


static void
chatty_check_for_emoticon (ChattyConversation *chatty_conv)
{
  GtkTextIter         start, end, position;
  GHashTableIter      iter;
  gpointer            key, value;
  char               *text;

  gtk_text_buffer_get_bounds (chatty_conv->msg_buffer,
                              &start,
                              &end);

  text = gtk_text_buffer_get_text (chatty_conv->msg_buffer,
                                   &start, &end,
                                   FALSE);

  g_hash_table_iter_init (&iter, ht_emoticon);

  while (g_hash_table_iter_next (&iter, &key, &value)) {
    if (g_str_has_suffix (text, (char*)key)) {
      position = end;

      gtk_text_iter_backward_chars (&position, strlen ((char*)key));
      gtk_text_buffer_delete (chatty_conv->msg_buffer, &position, &end);
      gtk_text_buffer_insert (chatty_conv->msg_buffer, &position, (char*)value, -1);
    }

  }

  g_free (text);
}


static void
chatty_update_typing_status (ChattyConversation *chatty_conv)
{
  PurpleConversation     *conv;
  PurpleConversationType  type;
  PurpleConvIm           *im;
  GtkTextIter             start, end;
  char                   *text;
  gboolean                empty;

  conv = chatty_conv->active_conv;

  type = purple_conversation_get_type (conv);

  if (type != PURPLE_CONV_TYPE_IM) {
    return;
  }

  gtk_text_buffer_get_bounds (chatty_conv->msg_buffer,
                              &start,
                              &end);

  text = gtk_text_buffer_get_text (chatty_conv->msg_buffer,
                                   &start, &end,
                                   FALSE);

  empty = (!text || !*text || (*text == '/'));

  im = PURPLE_CONV_IM(conv);

  if (!empty) {
    gboolean send = (purple_conv_im_get_send_typed_timeout (im) == 0);

    purple_conv_im_stop_send_typed_timeout (im);
    purple_conv_im_start_send_typed_timeout (im);

    if (send || (purple_conv_im_get_type_again (im) != 0 &&
        time(NULL) > purple_conv_im_get_type_again (im))) {

      unsigned int timeout;

      timeout = serv_send_typing (purple_conversation_get_gc (conv),
                                  purple_conversation_get_name (conv),
                                  PURPLE_TYPING);

      purple_conv_im_set_type_again (im, timeout);
    }
  } else {
    purple_conv_im_stop_send_typed_timeout (im);

    serv_send_typing (purple_conversation_get_gc (conv),
                      purple_conversation_get_name (conv),
                      PURPLE_NOT_TYPING);
  }

  g_free (text);
}


static gchar *
chatty_conv_check_for_links (const gchar *message)
{
  gchar  *msg_html;
  gchar  *msg_xhtml = NULL;

  msg_html = purple_markup_linkify (message);
  // convert all tags to lowercase for GtkLabel markup parser
  purple_markup_html_to_xhtml (msg_html, &msg_xhtml, NULL);

  g_free (msg_html);

  return msg_xhtml;
}


static PurpleCmdRet
cb_chatty_cmd (PurpleConversation  *conv,
               const gchar         *cmd,
               gchar              **args,
               gchar              **error,
               void                *data)
{
  char *msg = NULL;

  if (!g_strcmp0 (args[0], "help")) {
    msg = g_strdup ("Commands for setting properties:\n\n"
                    "General settings:\n"
                    " - '/chatty help': Displays this message.\n"
                    " - '/chatty emoticons [on; off]': Convert emoticons\n"
                    " - '/chatty return_sends [on; off]': Return = send message\n"
                    "\n"
                    "XMPP settings:\n"
                    " - '/chatty show_offline [on; off]': Show offline contacts\n"
                    " - '/chatty grey_offline [on; off]': Greyout offline-contacts\n"
                    " - '/chatty blur_idle [on; off]': Blur idle-contacts icons\n"
                    " - '/chatty typing_info [on; off]': Send typing notifications\n"
                    " - '/chatty msg_receipts [on; off]': Send message receipts\n"
                    " - '/chatty msg_carbons [on; off]': Share chat history\n");
  } else if (!g_strcmp0 (args[1], "on")) {
    if (!g_strcmp0 (args[0], "return_sends")) {
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/conversations/return_sends", TRUE);
      msg = g_strdup ("Return key sends messages");
    } else if (!g_strcmp0 (args[0], "show_offline")) {
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/blist/show_offline_buddies", TRUE);
      msg = g_strdup ("Offline contacts will be shown");
    } else if (!g_strcmp0 (args[0], "grey_offline")) {
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/blist/greyout_offline_buddies", TRUE);
      msg = g_strdup ("Offline user avatars will be greyed out");
    } else if (!g_strcmp0 (args[0], "blur_idle")) {
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/blist/blur_idle_buddies", TRUE);
      msg = g_strdup ("Offline user avatars will be blurred");
    } else if (!g_strcmp0 (args[0], "typing_info")) {
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/conversations/send_typing", TRUE);
      msg = g_strdup ("Typing messages will be sent");
    } else if (!g_strcmp0 (args[0], "msg_receipts")) {
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/conversations/msg_receipts", TRUE);
      msg = g_strdup ("Message receipts will be sent");
    } else if (!g_strcmp0 (args[0], "msg_carbons")) {
      chatty_purple_load_plugin ("core-riba-carbons");
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/plugins/message_carbons", TRUE);
      msg = g_strdup ("Chat history will be shared");
    } else if (!g_strcmp0 (args[0], "emoticons")) {
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/conversations/convert_emoticons", TRUE);
      msg = g_strdup ("Emoticons will be converted");
    }
  } else if (!g_strcmp0 (args[1], "off")) {
    if (!g_strcmp0 (args[0], "return_sends")) {
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/conversations/return_sends", FALSE);
      msg = g_strdup ("Return key doesn't send messages");
    } else if (!g_strcmp0 (args[0], "show_offline")) {
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/blist/show_offline_buddies", FALSE);
      msg = g_strdup("Offline contacts will be hidden");
    } else if (!g_strcmp0 (args[0], "grey_offline")) {
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/blist/greyout_offline_buddies", FALSE);
      msg = g_strdup ("Offline user avatars will not be greyed out");
    } else if (!g_strcmp0 (args[0], "blur_idle")) {
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/blist/blur_idle_buddies", FALSE);
      msg = g_strdup ("Offline user avatars will not be blurred");
    } else if (!g_strcmp0 (args[0], "typing_info")) {
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/conversations/send_typing", FALSE);
      msg = g_strdup ("Typing messages will be hidden");
    } else if (!g_strcmp0 (args[0], "msg_receipts")) {
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/conversations/msg_receipts", FALSE);
      msg = g_strdup ("Message receipts won't be sent");
    } else if (!g_strcmp0 (args[0], "msg_carbons")) {
      chatty_purple_unload_plugin ("core-riba-carbons");
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/plugins/message_carbons", FALSE);
      msg = g_strdup ("Chat history won't be shared");
    } else if (!g_strcmp0 (args[0], "emoticons")) {
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/conversations/convert_emoticons", FALSE);
      msg = g_strdup ("emoticons will not be converted");
    }
  }

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
 * chatty_conv_check_for_command:
 * @conv: a PurpleConversation
 *
 * Checks message for being a command
 * indicated by a "/" prefix
 *
 */
static gboolean
chatty_conv_check_for_command (PurpleConversation *conv)
{
  ChattyConversation *chatty_conv;
  gchar              *cmd;
  const gchar        *prefix;
  gboolean            retval = FALSE;
  GtkTextIter         start, end;
  PurpleMessageFlags  flags = 0;

  chatty_conv = CHATTY_CONVERSATION(conv);

  prefix = "/";

  flags |= PURPLE_MESSAGE_NO_LOG | PURPLE_MESSAGE_SYSTEM;

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
      purple_conversation_write (conv,
                                 "",
                                 "Nothing happens",
                                 flags,
                                 time(NULL));

      g_free (cmd);
      return TRUE;
    }

    purple_conversation_write (conv,
                               "",
                               cmdline,
                               flags,
                               time(NULL));

    status = purple_cmd_do_command (conv, cmdline, cmdline, &error);

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

        if ((prpl_info != NULL) &&
            (prpl_info->options & OPT_PROTO_SLASH_COMMANDS_NATIVE)) {
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
                                       "Unknown command.",
                                       flags,
                                       time(NULL));
            retval = TRUE;
          }
        }
        break;
      }
      case PURPLE_CMD_STATUS_WRONG_ARGS:
        purple_conversation_write (conv,
                                   "",
                                   "Wrong number of arguments for the command.",
                                   flags,
                                   time(NULL));
        retval = TRUE;
        break;
      case PURPLE_CMD_STATUS_FAILED:
        purple_conversation_write (conv,
                                   "",
                                   error ? error : "The command failed.",
                                   flags,
                                   time(NULL));
        g_free(error);
        retval = TRUE;
        break;
      case PURPLE_CMD_STATUS_WRONG_TYPE:
        if (purple_conversation_get_type (conv) == PURPLE_CONV_TYPE_IM)
          purple_conversation_write (conv,
                                     "",
                                     "That command only works in chats, not IMs.",
                                     flags,
                                     time(NULL));
        else
          purple_conversation_write (conv,
                                     "",
                                     "That command only works in IMs, not chats.",
                                     flags,
                                     time(NULL));
        retval = TRUE;
        break;
      case PURPLE_CMD_STATUS_WRONG_PRPL:
        purple_conversation_write (conv,
                                   "",
                                   "That command doesn't work on this protocol.",
                                   flags,
                                   time(NULL));
        retval = TRUE;
        break;
      default:
        break;
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


/**
 * chatty_conv_parse_message:
 * @msg: a char pointer
 *
 * Parse a chat single chat log message
 *
 */
static ChattyLog*
chatty_conv_parse_message (const gchar* msg)
{
  ChattyLog     *log;
  char          *timestamp;
  g_auto(GStrv)  timesplit = NULL,
                 accountsplit = NULL,
                 namesplit = NULL;

  if (msg == NULL)
    return NULL;

  /* Separate the timestamp from the rest of the message */
  timesplit = g_strsplit (msg, ") ", 2);
  /* Format is '(x:y:z' */
  if (timesplit[0] == NULL || strlen (timesplit[0]) < 6)
    return NULL;

  timestamp = strchr(timesplit[0], '(');
  g_return_val_if_fail (timestamp != NULL, NULL);

  log = g_new0 (ChattyLog, 1);
  log->time_stamp = g_strdup(&timestamp[1]);

  if (timesplit[1] == NULL)
    return log;

  accountsplit = g_strsplit (timesplit[1], ": ", 2);

  if (accountsplit[0] == NULL)
    return log;

  namesplit = g_strsplit (accountsplit[0], "/", 2);
  log->name = g_strdup (namesplit[0]);

  if (accountsplit[1] == NULL)
    return log;

  log->msg = g_strdup (accountsplit[1]);
  return log;
}


/**
 * chatty_conv_message_get_last_msg:
 * @buddy: a PurpleBuddy
 *
 * Get the last message from log
 *
 */
ChattyLog*
chatty_conv_message_get_last_msg (PurpleBuddy *buddy)
{
  GList         *history;
  ChattyLog     *log_data = NULL;
  PurpleAccount *account;
  g_auto(GStrv)  logs = NULL;
  gchar         *read_log;
  gchar         *stripped;
  const gchar   *b_name;
  int            num_logs;

  account = purple_buddy_get_account (buddy);
  b_name = purple_buddy_get_name (buddy);
  history = purple_log_get_logs (PURPLE_LOG_IM, b_name, account);

  if (history == NULL) {
    g_list_free (history);
    return NULL;
  }

  history = g_list_first (history);

  read_log = purple_log_read ((PurpleLog*)history->data, NULL);
  stripped = purple_markup_strip_html (read_log);

  logs = g_strsplit (stripped, "\n", -1);
  num_logs = g_strv_length (logs) - 2;

  log_data = chatty_conv_parse_message (logs[num_logs]);

  g_list_foreach (history, (GFunc)purple_log_free, NULL);
  g_list_free (history);
  g_free (read_log);
  g_free (stripped);

  return log_data;;
}


/**
 * chatty_conv_delete_message_history:
 * @buddy: a PurpleBuddy
 *
 * Delete all logs from the given buddyname
 *
 */
gboolean
chatty_conv_delete_message_history (PurpleBuddy *buddy)
{
  GList         *history;
  PurpleAccount *account;
  const gchar   *b_name;

  account = purple_buddy_get_account (buddy);
  b_name = purple_buddy_get_name (buddy);
  history = purple_log_get_logs (PURPLE_LOG_IM, b_name, account);

  if (history == NULL) {
    g_list_free (history);
    return FALSE;
  }

  for (int i = 0; history && i < MAX_MSGS; history = history->next) {
    purple_log_delete ((PurpleLog*)history->data);
  }

  g_list_foreach (history, (GFunc)purple_log_free, NULL);
  g_list_free (history);

  return TRUE;
}


/**
 * chatty_conv_add_message_history_to_conv:
 * @data: a ChattyConversation
 *
 * Parse the chat log and add the
 * messages to a message-list
 *
 */
static gboolean
chatty_conv_add_message_history_to_conv (gpointer data)
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
    PurpleBuddy   *buddy;
    gchar         *name = NULL;
    const gchar   *conv_name;
    const gchar   *b_name;
    gchar         *time_stamp;
    gchar         *msg_html;
    GList         *history;
    guint          msg_dir;

    conv_name = purple_conversation_get_name (chatty_conv->active_conv);
    account = purple_conversation_get_account (chatty_conv->active_conv);

    buddy = purple_find_buddy (account, conv_name);

    if (buddy == NULL) {
      return FALSE;
    }

    b_name = purple_buddy_get_name (buddy);
    history = purple_log_get_logs (PURPLE_LOG_IM, b_name, account);

    if (history == NULL) {
      g_list_free (history);
      return FALSE;
    }

    b_name = purple_buddy_get_alias (purple_find_buddy (account, conv_name));

    // limit the log-list to MAX_MSGS msgs since we currently have no
    // infinite scrolling implemented
    for (int i = 0; history && i < MAX_MSGS; history = history->next) {
      g_auto(GStrv) line_split = NULL;
      g_auto(GStrv) logs = NULL;
      g_autofree gchar *read_log = purple_log_read ((PurpleLog*)history->data, NULL);
      g_autofree gchar *stripped = purple_markup_strip_html (read_log);

      logs = g_strsplit (stripped, "\n", -1);

      for (int num = g_strv_length (logs) - 1; num >= 0; num--) {
        log_data = chatty_conv_parse_message (logs[num]);

        if (log_data) {
          i++;
          msgs = g_list_prepend (msgs, (gpointer)log_data);
        }
      }

      line_split = g_strsplit (b_name, "/", -1);
      name = g_strdup (line_split[0]);
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
        msg_dir = MSG_IS_INCOMING;
      } else {
        msg_dir = MSG_IS_OUTGOING;
      }

      msg_html = chatty_conv_check_for_links (log_data->msg);

      if (msg_html[0] != '\0') {
        chatty_msg_list_add_message (chatty_conv->msg_list,
                                     msg_dir,
                                     msg_html,
                                     time_stamp);
      }

      g_free (msg_html);
    }

    g_list_foreach (msgs, (GFunc)g_free, NULL);
    g_list_free (msgs);
    g_free (name);

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
  text = g_strdup_printf ("%s %s",name_split[0], " >");

  gtk_notebook_set_tab_label_text (GTK_NOTEBOOK(chatty->pane_view_message_list),
                                   chatty_conv->tab_cont, text);

  gtk_widget_show (chatty_conv->tab_cont);

  gtk_notebook_set_current_page (GTK_NOTEBOOK(chatty->pane_view_message_list), 0);

  if (purple_prefs_get_bool (CHATTY_PREFS_ROOT "/conversations/show_tabs")) {
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK(chatty->pane_view_message_list), TRUE);
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
  ChattyConversation  *chatty_conv;
  PurpleConnection    *gc;
  PurpleAccount       *account;
  gchar               *msg_html;

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
  g_return_if_fail (account != NULL);
  gc = purple_account_get_connection (account);
  g_return_if_fail (gc != NULL || !(flags & (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV)));

  if (*message != '\0') {
    if (flags & PURPLE_MESSAGE_RECV) {
      msg_html = chatty_conv_check_for_links (message);

      chatty_msg_list_add_message (chatty_conv->msg_list,
                                   MSG_IS_INCOMING,
                                   msg_html,
                                   NULL);

      g_free (msg_html);
    } else if (flags & PURPLE_MESSAGE_SYSTEM) {
      chatty_msg_list_add_message (chatty_conv->msg_list,
                                   MSG_IS_SYSTEM,
                                   message,
                                   NULL);
    }

    chatty_conv_set_unseen (chatty_conv, CHATTY_UNSEEN_NONE);
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
 */
static void
chatty_conv_switch_conv (ChattyConversation *chatty_conv)
{
  chatty_data_t *chatty = chatty_get_data();

  gtk_notebook_set_current_page (GTK_NOTEBOOK(chatty->pane_view_message_list),
                                 gtk_notebook_page_num (GTK_NOTEBOOK(chatty->pane_view_message_list),
                                 chatty_conv->tab_cont));

  gtk_widget_grab_focus (GTK_WIDGET(chatty_conv->msg_entry));
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
  chatty_data_t *chatty = chatty_get_data();

  gtk_notebook_remove_page (GTK_NOTEBOOK(chatty->pane_view_message_list),
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
chatty_conv_setup_pane (ChattyConversation *chatty_conv,
                        guint               msg_type)
{
  GtkBuilder      *builder;
  GtkWidget       *scrolled;
  GtkAdjustment   *vadjust;
  GtkWidget       *vbox;
  GtkWidget       *box;
  GtkWidget       *frame;
  GtkStyleContext *sc;

  gtk_icon_theme_add_resource_path (gtk_icon_theme_get_default (),
                                    "/sm/puri/chatty/icons/ui/");

  builder = gtk_builder_new_from_resource ("/sm/puri/chatty/ui/chatty-pane-msg-view.ui");

  chatty_conv->msg_entry = GTK_WIDGET(gtk_builder_get_object (builder, "text_input"));

  box = GTK_WIDGET(gtk_builder_get_object (builder, "msg_input_box"));
  frame = GTK_WIDGET(gtk_builder_get_object (builder, "frame"));
  scrolled = GTK_WIDGET(gtk_builder_get_object (builder, "scrolled"));
  chatty_conv->button_send = GTK_WIDGET(gtk_builder_get_object (builder, "button_send"));

  vadjust = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW(scrolled));

  g_signal_connect_after (G_OBJECT (vadjust),
                          "notify::upper",
                          G_CALLBACK (cb_msg_input_vadjust),
                          (gpointer) chatty_conv);

  chatty_conv->msg_scrolled = scrolled;
  chatty_conv->msg_frame = frame;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  chatty_conv->msg_buffer = gtk_text_buffer_new (NULL);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW(chatty_conv->msg_entry),
                            chatty_conv->msg_buffer);

  g_object_set_data (G_OBJECT(chatty_conv->msg_buffer),
                              "user_data", chatty_conv);

  g_signal_connect (G_OBJECT(chatty_conv->msg_entry),
                    "key-press-event",
                    G_CALLBACK(cb_textview_key_pressed),
                    (gpointer) chatty_conv);

  g_signal_connect (G_OBJECT(chatty_conv->msg_entry),
                    "key-release-event",
                    G_CALLBACK(cb_textview_key_released),
                    (gpointer) chatty_conv);

  g_signal_connect (G_OBJECT(chatty_conv->msg_entry),
                    "focus-in-event",
                    G_CALLBACK(cb_textview_focus_in),
                    (gpointer) frame);

  g_signal_connect (G_OBJECT(chatty_conv->msg_entry),
                    "focus-out-event",
                    G_CALLBACK(cb_textview_focus_out),
                    (gpointer) frame);

  sc = gtk_widget_get_style_context (chatty_conv->button_send);

  switch (msg_type) {
    case MSG_TYPE_SMS:
      gtk_style_context_add_class (sc, "button_send_green");
      break;
    case MSG_TYPE_IM:
    case MSG_TYPE_IM_E2EE:
      gtk_style_context_add_class (sc, "button_send_blue");
      break;
    case MSG_TYPE_UNKNOWN:
      break;
    default:
      break;
  }

  g_signal_connect (chatty_conv->button_send,
                    "clicked",
                    G_CALLBACK(cb_button_send_clicked),
                    (gpointer) chatty_conv);

  chatty_conv->msg_list = CHATTY_MSG_LIST (chatty_msg_list_new (msg_type, FALSE));

  g_signal_connect (chatty_conv->msg_list,
                    "message-added",
                    G_CALLBACK(cb_msg_list_message_added),
                    (gpointer) chatty_conv);

  gtk_box_pack_start (GTK_BOX (vbox),
                      GTK_WIDGET (chatty_conv->msg_list),
                      TRUE, TRUE, 0);

  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);

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
  PurpleAccount      *account;
  PurpleBuddy        *buddy;
  PurpleValue        *value;
  PurpleBlistNode    *conv_node;
  ChattyConversation *chatty_conv;
  const gchar        *protocol_id;
  const gchar        *conv_name;
  guint               msg_type;

  PurpleConversationType conv_type = purple_conversation_get_type (conv);

  if (conv_type == PURPLE_CONV_TYPE_IM && (chatty_conv = chatty_conv_find_conv (conv))) {
    conv->ui_data = chatty_conv;

    chatty_conv_switch_active_conversation (conv);

    return;
  }

  chatty_conv = g_new0 (ChattyConversation, 1);
  conv->ui_data = chatty_conv;
  chatty_conv->active_conv = conv;

  account = purple_conversation_get_account (conv);
  protocol_id = purple_account_get_protocol_id (account);

  if (conv_type == PURPLE_CONV_TYPE_IM) {
    chatty_conv->conv_header = g_malloc0 (sizeof (ChattyConvViewHeader));
  }

  // A new SMS contact must be added directly to the
  // roster, without buddy-request confirmation
  if (g_strcmp0 (protocol_id, "prpl-mm-sms") == 0) {
    msg_type = MSG_TYPE_SMS;

    conv_name = purple_conversation_get_name (conv);

    buddy = purple_find_buddy (account, conv_name);

    if (buddy == NULL) {
      buddy = purple_buddy_new (account, conv_name, NULL);
      purple_blist_add_buddy (buddy, NULL, NULL, NULL);

      g_debug ("Buddy SMS: %s added to roster", purple_buddy_get_name (buddy));
    }
  } else {
    msg_type = MSG_TYPE_IM;
  }

  chatty_conv->tab_cont = chatty_conv_setup_pane (chatty_conv, msg_type);
  g_object_set_data (G_OBJECT(chatty_conv->tab_cont),
                     "ChattyConversation",
                     chatty_conv);

  gtk_widget_hide (chatty_conv->button_send);
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

  chatty_conv->attach.timer = g_idle_add (chatty_conv_add_message_history_to_conv, chatty_conv);

  if (CHATTY_IS_CHATTY_CONVERSATION (conv)) {
    purple_signal_emit (chatty_conversations_get_handle (),
                        "conversation-displayed",
                        CHATTY_CONVERSATION (conv));
  }
}


static gboolean
cb_ht_check_items (gpointer key,
                   gpointer value,
                   gpointer user_data)
{
  return ((GtkWidget*)value == user_data) ? TRUE : FALSE;
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

  if (chatty_conv->attach.timer) {
    g_source_remove(chatty_conv->attach.timer);
  }

  g_hash_table_foreach_remove (ht_sms_id,
                               cb_ht_check_items,
                               chatty_conv->msg_bubble_footer);

  g_hash_table_destroy (ht_sms_id);

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

  g_signal_connect (G_OBJECT(chatty->pane_view_message_list),
                    "switch_page",
                    G_CALLBACK(cb_stack_cont_before_switch_conv), 0);

  g_signal_connect_after (G_OBJECT(chatty->pane_view_message_list),
                          "switch_page",
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
  purple_prefs_add_bool (CHATTY_PREFS_ROOT "/conversations/im/show_buddy_icons", TRUE);
  purple_prefs_add_bool (CHATTY_PREFS_ROOT "/conversations/show_timestamps", TRUE);
  purple_prefs_add_bool (CHATTY_PREFS_ROOT "/conversations/show_tabs", FALSE);
  purple_prefs_add_bool (CHATTY_PREFS_ROOT "/conversations/send_typing", TRUE);
  purple_prefs_add_bool (CHATTY_PREFS_ROOT "/conversations/convert_emoticons", TRUE);
  purple_prefs_add_bool (CHATTY_PREFS_ROOT "/conversations/send_receipts", TRUE);
  purple_prefs_add_bool (CHATTY_PREFS_ROOT "/conversations/return_sends", TRUE);

  purple_prefs_add_bool ("/purple/logging/log_system", FALSE);
  purple_prefs_set_bool ("/purple/logging/log_system", FALSE);

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
                         "sms-sent", &handle,
                         PURPLE_CALLBACK (cb_sms_show_send_receipt), NULL);

  purple_signal_connect (purple_conversations_get_handle (),
                         "buddy-typing", &handle,
                         PURPLE_CALLBACK (cb_buddy_typing), NULL);

  purple_signal_connect (purple_conversations_get_handle (),
                         "buddy-typed", &handle,
                         PURPLE_CALLBACK (cb_buddy_typed), NULL);

  purple_signal_connect (purple_conversations_get_handle (),
                         "buddy-typing-stopped", &handle,
                         PURPLE_CALLBACK (cb_buddy_typing_stopped), NULL);

  purple_signal_connect (chatty_conversations_get_handle(),
                         "conversation-switched",
                         handle, PURPLE_CALLBACK (cb_conversation_switched), NULL);

  purple_cmd_register ("chatty",
                       "ww",
                       PURPLE_CMD_P_DEFAULT,
                       PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS,
                       NULL,
                       cb_chatty_cmd,
                       "chatty &lt;help&gt;:  "
                       "For a list of commands use the 'help' argument.",
                       NULL);

  ht_sms_id  = g_hash_table_new_full (g_str_hash,
                                      g_str_equal,
                                      g_free,
                                      NULL);

  chatty_conv_init_emoticon_translations ();
}


void
chatty_conversations_uninit (void)
{
  g_hash_table_destroy (ht_emoticon);
  purple_prefs_disconnect_by_handle (chatty_conversations_get_handle());
  purple_signals_disconnect_by_handle (chatty_conversations_get_handle());
  purple_signals_unregister_by_instance (chatty_conversations_get_handle());
}
