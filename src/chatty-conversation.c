/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#define G_LOG_DOMAIN "chatty-conversation"

#include <glib/gi18n.h>
#include "chatty-window.h"
#include "chatty-icons.h"
#include "chatty-lurch.h"
#include "chatty-purple-init.h"
#include "chatty-message-list.h"
#include "chatty-conversation.h"
#include "chatty-history.h"

#define MAX_GMT_ISO_SIZE 256
#define MAX_MSGS 50

static GHashTable *ht_sms_id = NULL;
static GHashTable *ht_emoticon = NULL;

const char *avatar_colors[16] = {"E57373", "F06292", "BA68C8", "9575CD",
                                 "7986CB", "64B5F6", "4FC3F7", "4DD0E1",
                                 "4DB6AC", "81C784", "AED581", "DCE775",
                                 "FFD54F", "FFB74D", "FF8A65", "A1887F"};

static void
chatty_conv_write_conversation (PurpleConversation *conv,
                                const char         *who,
                                const char         *alias,
                                const char         *message,
                                PurpleMessageFlags  flags,
                                time_t              mtime);


void chatty_conv_new (PurpleConversation *conv);
static gboolean chatty_conv_check_for_command (PurpleConversation *conv);
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

  if (chatty_conv && chatty_conv->conv == conv) {
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
  ChattyConversation  *chatty_conv;

  chatty_conv  = (ChattyConversation *)data;

  if (chatty_conv->msg_bubble_footer != NULL) {
    gtk_box_pack_start (GTK_BOX(bubble),
                        chatty_conv->msg_bubble_footer,
                        FALSE, FALSE, 10);
  }
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
  gchar               *footer_str = NULL;
  const gchar         *protocol_id;
  gchar               *sms_id_str;
  guint                sms_id;

  chatty_conv  = (ChattyConversation *)data;
  conv = chatty_conv->conv;

  account = purple_conversation_get_account (conv);

  gtk_text_buffer_get_bounds (chatty_conv->input.buffer, &start, &end);

  if (chatty_conv_check_for_command (conv)) {
    gtk_widget_hide (chatty_conv->input.button_send);
    gtk_text_buffer_delete (chatty_conv->input.buffer, &start, &end);
    return;
  }

  if (!purple_account_is_connected (account)) {
    return;
  }

  protocol_id = purple_account_get_protocol_id (account);

  gtk_widget_grab_focus (chatty_conv->input.entry);

  purple_idle_touch ();

  message = gtk_text_buffer_get_text (chatty_conv->input.buffer,
                                      &start,
                                      &end,
                                      FALSE);

  footer_str = g_strconcat ("<small>",
                            "<span color='grey'>",
                            " ✓",
                            "</span></small>",
                            NULL);

  chatty_conv->msg_bubble_footer = GTK_WIDGET(gtk_label_new (NULL));
  gtk_label_set_markup (GTK_LABEL(chatty_conv->msg_bubble_footer), footer_str);
  gtk_label_set_xalign (GTK_LABEL(chatty_conv->msg_bubble_footer), 1);

  if (gtk_text_buffer_get_char_count (chatty_conv->input.buffer)) {
    // provide a msg-id to the sms-plugin for send-receipts
    if (g_strcmp0 (protocol_id, "prpl-mm-sms") == 0) {
      sms_id = g_random_int ();

      sms_id_str = g_strdup_printf ("%i", sms_id);

      g_hash_table_insert (ht_sms_id,
                           sms_id_str, chatty_conv->msg_bubble_footer);

      g_debug ("hash table insert sms_id_str: %s  ht_size: %i\n",
               sms_id_str, g_hash_table_size (ht_sms_id));

      purple_conv_im_send_with_flags (PURPLE_CONV_IM (conv),
                                      sms_id_str,
                                      PURPLE_MESSAGE_NO_LOG |
                                      PURPLE_MESSAGE_NOTIFY |
                                      PURPLE_MESSAGE_INVISIBLE);
    }

    if (purple_conversation_get_type (conv) == PURPLE_CONV_TYPE_IM) {
      purple_conv_im_send (PURPLE_CONV_IM(conv), message);
    } else if (purple_conversation_get_type (conv) == PURPLE_CONV_TYPE_CHAT) {
      purple_conv_chat_send(PURPLE_CONV_CHAT(conv), message);
    }

    gtk_widget_hide (chatty_conv->input.button_send);
  }

  gtk_text_buffer_delete (chatty_conv->input.buffer, &start, &end);

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
  if (!(key_event->state & GDK_SHIFT_MASK) && key_event->keyval == GDK_KEY_Return) {
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

  if (gtk_text_buffer_get_char_count (chatty_conv->input.buffer)) {
    gtk_widget_show (chatty_conv->input.button_send);
  } else {
    gtk_widget_hide (chatty_conv->input.button_send);
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

  conv = chatty_conv->conv;

  g_return_if_fail (conv != NULL);

  g_debug ("cb_stack_cont_switch_conv conv: chatty_conv->conv: %s",
           purple_conversation_get_name (conv));

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

  vadjust = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW(chatty_conv->input.scrolled));
  vscroll = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW(chatty_conv->input.scrolled));
  upper = gtk_adjustment_get_upper (GTK_ADJUSTMENT(vadjust));
  page_size = gtk_adjustment_get_page_size (GTK_ADJUSTMENT(vadjust));
  max_height = gtk_scrolled_window_get_max_content_height (GTK_SCROLLED_WINDOW(chatty_conv->input.scrolled));

  gtk_adjustment_set_value (vadjust, upper - page_size);

  if (upper > (gdouble)max_height) {
    gtk_widget_set_visible (vscroll, TRUE);
  } else {
    gtk_widget_set_visible (vscroll, FALSE);
  }

  gtk_widget_queue_draw (chatty_conv->input.frame);
}


static void
cb_tree_view_row_activated (GtkTreeView       *treeview,
                            GtkTreePath       *path,
                            GtkTreeViewColumn *column,
                            gpointer           user_data)
{

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

  gtk_text_buffer_get_bounds (chatty_conv->input.buffer,
                              &start,
                              &end);

  text = gtk_text_buffer_get_text (chatty_conv->input.buffer,
                                   &start, &end,
                                   FALSE);

  g_hash_table_iter_init (&iter, ht_emoticon);

  while (g_hash_table_iter_next (&iter, &key, &value)) {
    if (g_str_has_suffix (text, (char*)key)) {
      position = end;

      gtk_text_iter_backward_chars (&position, strlen ((char*)key));
      gtk_text_buffer_delete (chatty_conv->input.buffer, &position, &end);
      gtk_text_buffer_insert (chatty_conv->input.buffer, &position, (char*)value, -1);
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

  conv = chatty_conv->conv;

  type = purple_conversation_get_type (conv);

  if (type != PURPLE_CONV_TYPE_IM) {
    return;
  }

  gtk_text_buffer_get_bounds (chatty_conv->input.buffer,
                              &start,
                              &end);

  text = gtk_text_buffer_get_text (chatty_conv->input.buffer,
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

  msg_html = purple_markup_strip_html  (message);
  msg_html = purple_markup_linkify (msg_html);
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


  if (args[0] == NULL || !g_strcmp0 (args[0], "help")) {
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
    } else if (!g_strcmp0 (args[0], "welcome")) {
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/status/first_start", TRUE);
      msg = g_strdup ("Welcome screen has been reset");
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

  gtk_text_buffer_get_bounds (chatty_conv->input.buffer,
                              &start,
                              &end);

  cmd = gtk_text_buffer_get_text (chatty_conv->input.buffer,
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
 * Returns: (transfer-full): a #ChattyLog
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

  g_return_val_if_fail (num_logs >= 2, NULL);

  log_data = chatty_conv_parse_message (logs[num_logs]);

  g_list_free_full (history, (GDestroyNotify)purple_log_free);
  g_free (read_log);
  g_free (stripped);

  return log_data;;
}


/**
 * chatty_conv_muc_get_avatar_color:
 * @user_id: a const char
 *
 * Picks a color for muc user avatars
 *
 */
static gchar *
chatty_conv_muc_get_avatar_color (const char *user_id)
{
  const char   *color;
  guint   hash = 0U;

  hash = g_str_hash (user_id);

  color = avatar_colors[hash % G_N_ELEMENTS (avatar_colors)];

  return g_strdup (color);
}


/**
 * chatty_conv_delete_message_history:
 * @buddy: a PurpleBuddy
 *
 * Delete all logs from the given buddyname
 *
 */
// TODO: LELAND: Can I remove this method now?
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

  g_list_free_full (history, (GDestroyNotify)purple_log_free);

  return TRUE;
}

static void
chatty_conv_get_im_messages_cb (const unsigned char* msg,
                                int direction,
                                time_t time_stamp,
                                ChattyConversation *chatty_conv,
                                int last_message){
  gchar   *msg_html;
  guint    msg_dir;

  if (direction == 1) {
    msg_dir = MSG_IS_INCOMING;
  } else if(direction == -1){
    msg_dir = MSG_IS_OUTGOING;
  } else {
    msg_dir = MSG_IS_SYSTEM; // TODO: LELAND: Do we have this case for IMs?
  }

  msg_html = chatty_conv_check_for_links ((const gchar*)msg);

  if (msg_html[0] != '\0') {
    chatty_msg_list_add_message (chatty_conv->msg_list,
                                 msg_dir,
                                 msg_html,
                                 last_message ? asctime(localtime(&time_stamp)) : NULL, // last seen message
                                 NULL);
  }

  g_free (msg_html);

  g_object_set_data (G_OBJECT (chatty_conv->input.entry),
                     "attach-start-time",
                     NULL);


}


static void
chatty_conv_get_chat_messages_cb (const unsigned char* msg,
                                  int time_stamp,
                                  int direction,
                                  const char* room,
                                  const unsigned char *who,
                                  ChattyConversation *chatty_conv){
  gchar   *msg_html;
  PurpleBuddy              *buddy;
  const char               *color;
  GdkPixbuf                *avatar = NULL;
  GtkWidget                *icon = NULL;
  PurpleAccount            *account;
  gchar                    *alias;
  gchar                    **line_split;

  msg_html = chatty_conv_check_for_links ((const gchar*)msg);

  if (msg_html[0] != '\0') {

    if (direction == 1) {
      msg_html = chatty_conv_check_for_links ((const gchar*)msg);

      account = purple_conversation_get_account (chatty_conv->conv);
      buddy = purple_find_buddy (account, (const gchar*)room);
      color = chatty_conv_muc_get_avatar_color ((const gchar*)room);

      // Extract the alias from 'who' (full_room_address/alias)
      line_split = g_strsplit ((const gchar*)who, "/", -1);
      alias = g_strdup(line_split[1]);

      avatar = chatty_icon_get_buddy_icon ((PurpleBlistNode*)buddy,
                                           (const gchar*)alias,
                                           CHATTY_ICON_SIZE_MEDIUM,
                                           color,
                                           FALSE);
      g_strfreev(line_split);
      g_free(alias);

      if (avatar) {
        icon = gtk_image_new_from_pixbuf (avatar);
        g_object_unref (avatar);
      }

      chatty_msg_list_add_message (chatty_conv->msg_list,
                                   MSG_IS_INCOMING,
                                   msg_html,
                                   NULL,
                                   icon ? icon : NULL);

    } else if (direction == -1) {
      msg_html = chatty_conv_check_for_links ((const gchar*)msg);

      chatty_msg_list_add_message (chatty_conv->msg_list,
                                   MSG_IS_OUTGOING,
                                   msg_html,
                                   NULL,
                                   NULL);
    } else {
      chatty_msg_list_add_message (chatty_conv->msg_list,
                                   MSG_IS_SYSTEM,
                                   (const gchar*)msg,
                                   NULL,
                                   NULL);
    }

    chatty_conv_set_unseen (chatty_conv, CHATTY_UNSEEN_NONE);

  }

  g_free (msg_html);

  g_object_set_data (G_OBJECT (chatty_conv->input.entry),
                     "attach-start-time",
                     NULL);

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
  PurpleAccount *account;
  const gchar   *conv_name;
  gboolean       im;
  ChattyConversation *chatty_conv = data;
  gchar         *who;
  gchar         **line_split;

  im = (chatty_conv->conv->type == PURPLE_CONV_TYPE_IM);

  g_source_remove (chatty_conv->attach.timer);
  chatty_conv->attach.timer = 0;

  conv_name = purple_conversation_get_name (chatty_conv->conv);
  account = purple_conversation_get_account (chatty_conv->conv);

  if (im) {
    // Remove resource (user could be connecting from different devices/applications)
    // TODO: LELAND: This should be an utility fn. Does libpurple offer a helper for this?
    line_split = g_strsplit (conv_name, "/", -1);
    who = g_strdup(line_split[0]);

    chatty_history_get_im_messages (account->username, who, chatty_conv_get_im_messages_cb, chatty_conv);
  }else{
    chatty_history_get_chat_messages (account->username, conv_name, chatty_conv_get_chat_messages_cb, chatty_conv);
  }

  return FALSE;
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

  return chatty_conv ? chatty_conv->conv : NULL;
}


/**
 * chatty_conv_muc_list_add_columns:
 * @treeview: a GtkTreeView
 *
 * Setup columns for muc list treeview.
 *
 */
static void
chatty_conv_muc_list_add_columns (GtkTreeView *treeview)
{
  GtkCellRenderer   *renderer;
  GtkTreeViewColumn *column;

  renderer = gtk_cell_renderer_pixbuf_new ();
  column = gtk_tree_view_column_new_with_attributes ("Avatar",
                                                     renderer,
                                                     "pixbuf",
                                                     MUC_COLUMN_AVATAR,
                                                     NULL);

  gtk_cell_renderer_set_padding (renderer, 12, 12);
  gtk_tree_view_append_column (treeview, column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Name",
                                                     renderer,
                                                     "text",
                                                     MUC_COLUMN_ENTRY,
                                                     NULL);

  gtk_tree_view_column_set_attributes (column, renderer,
                                       "markup", MUC_COLUMN_ENTRY,
                                       NULL);

  g_object_set (renderer,
                // TODO derive width-chars from screen width
                "width-chars", 24,
                "ellipsize", PANGO_ELLIPSIZE_END,
                NULL);

  gtk_cell_renderer_set_alignment (renderer, 0.0, 0.4);
  gtk_tree_view_append_column (treeview, column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Time",
                                                     renderer,
                                                     "text",
                                                     MUC_COLUMN_LAST,
                                                     NULL);

  gtk_tree_view_column_set_attributes (column,
                                       renderer,
                                       "markup", MUC_COLUMN_LAST,
                                       NULL);

  g_object_set (renderer,
                "xalign", 0.95,
                "yalign", 0.2,
                NULL);

  gtk_tree_view_append_column (treeview, column);
}


/**
 * chatty_conv_sort_muc_list:
 * @model:    a GtkTreeModel
 * @a, b:     a GtkTreeIter
 * @userdata: a gpointer
 *
 * Sorts the muc list according
 * to user level
 *
 * Function is called from
 * chatty_conv_create_muc_list
 *
 */
static gint
chatty_conv_sort_muc_list (GtkTreeModel *model,
                           GtkTreeIter  *a,
                           GtkTreeIter  *b,
                           gpointer userdata)
{
  PurpleConvChatBuddyFlags f1 = 0, f2 = 0;
  char                     *user1 = NULL, *user2 = NULL;
  gint                     ret = 0;

  gtk_tree_model_get (model, a,
                      MUC_COLUMN_ALIAS_KEY, &user1,
                      MUC_COLUMN_FLAGS, &f1,
                      -1);

  gtk_tree_model_get (model, b,
                      MUC_COLUMN_ALIAS_KEY, &user2,
                      MUC_COLUMN_FLAGS, &f2,
                      -1);

  /* Only sort by membership levels */
  f1 &= PURPLE_CBFLAGS_VOICE | PURPLE_CBFLAGS_HALFOP |
        PURPLE_CBFLAGS_OP | PURPLE_CBFLAGS_FOUNDER;

  f2 &= PURPLE_CBFLAGS_VOICE | PURPLE_CBFLAGS_HALFOP |
        PURPLE_CBFLAGS_OP | PURPLE_CBFLAGS_FOUNDER;

  ret = g_strcmp0 (user1, user2);

  if (user1 != NULL && user2 != NULL) {
    if (f1 != f2) {
      ret = (f1 > f2) ? -1 : 1;
    }
  }

  g_free (user1);
  g_free (user2);

  return ret;
}


/**
 * chatty_conv_create_muc_list:
 * @chatty_conv: a ChattyConversation
 *
 * Sets up treeview for muc user list
 * Function is called from chatty_conv_new
 *
 */
static void
chatty_conv_create_muc_list (ChattyConversation *chatty_conv)
{
  GtkTreeView     *treeview;
  GtkListStore    *treemodel;
  GtkStyleContext *sc;

  treemodel = gtk_list_store_new (MUC_NUM_COLUMNS,
                                  G_TYPE_OBJECT,
                                  G_TYPE_STRING,
                                  G_TYPE_STRING,
                                  G_TYPE_STRING,
                                  G_TYPE_STRING,
                                  G_TYPE_INT);

  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE(treemodel),
                                   MUC_COLUMN_ALIAS_KEY,
                                   chatty_conv_sort_muc_list,
                                   NULL, NULL);

  treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model (GTK_TREE_MODEL(treemodel)));

  gtk_tree_view_set_grid_lines (treeview, GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
  gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW(treeview), TRUE);
  sc = gtk_widget_get_style_context (GTK_WIDGET(treeview));
  gtk_style_context_add_class (sc, "list_no_select");
  g_signal_connect (treeview,
                    "row-activated",
                    G_CALLBACK (cb_tree_view_row_activated),
                    NULL);

  gtk_tree_view_set_headers_visible (treeview, FALSE);
  chatty_conv_muc_list_add_columns (GTK_TREE_VIEW (treeview));
  gtk_tree_view_columns_autosize (GTK_TREE_VIEW (treeview));
  chatty_conv->muc.treeview = treeview;

  chatty_conv->muc.list = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);

  gtk_box_pack_start (GTK_BOX (chatty_conv->muc.list),
                      GTK_WIDGET (treeview),
                      TRUE, TRUE, 0);

  gtk_widget_show_all (GTK_WIDGET(chatty_conv->muc.list));
}


/**
 * chatty_conv_muc_get_user_status:
 * @chat:  a PurpleConvChat
 * @name:  a const char
 * @flags  PurpleConvChatBuddyFlags
 *
 * Retrieve user status from buddy flags
 *
 * called from chatty_conv_muc_add_user
 *
 */
static char *
chatty_conv_muc_get_user_status (PurpleConvChat          *chat,
                                 const char              *name,
                                 PurpleConvChatBuddyFlags flags)
{
  const char *color_tag;
  const char *status;
  char       *text;

  if (flags & PURPLE_CBFLAGS_FOUNDER) {
    status = _("Owner");
    color_tag = "<span color='#4d86ff'>";
  } else if (flags & PURPLE_CBFLAGS_OP) {
    status = _("Moderator");
    color_tag = "<span color='#66e6ff'>";
  } else if (flags & PURPLE_CBFLAGS_VOICE) {
    status = _("Member");
    color_tag = "<span color='#c0c0c0'>";
  } else {
    color_tag = "<span color='#000000'>";
    status = "";
  }

  text = g_strconcat (color_tag, status, "</span>", NULL);

  return text;
}


/**
 * chatty_conv_muc_add_user:
 * @conv:     a ChattyConversation
 * @cb:       a PurpleConvChatBuddy
 *
 * Add a user to the muc list
 *
 * called from chatty_conv_muc_list_add_users
 *
 */
static void
chatty_conv_muc_add_user (PurpleConversation  *conv,
                          PurpleConvChatBuddy *cb)
{
  ChattyConversation       *chatty_conv;
  PurpleAccount            *account;
  PurpleBuddy              *buddy;
  PurpleConvChat           *chat;
  PurpleConnection         *gc;
  PurplePluginProtocolInfo *prpl_info;
  GtkTreeModel             *treemodel;
  GtkListStore             *liststore;
  GdkPixbuf                *avatar = NULL;
  GtkTreePath              *path;
  GtkTreeIter               iter;
  gchar                    *status;
  const gchar              *text;
  const gchar              *color;
  const gchar              *name, *alias;
  gchar                    *tmp, *alias_key;
  gchar                    *real_who = NULL;
  PurpleConvChatBuddyFlags  flags;
  int                       chat_id;

  alias = cb->alias;
  name  = cb->name;
  flags = cb->flags;

  chat = PURPLE_CONV_CHAT(conv);
  chatty_conv = CHATTY_CONVERSATION(conv);
  gc = purple_conversation_get_gc (conv);

  if (!gc || !(prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl))) {
    return;
  }

  g_debug ("chatty_conv_muc_add_user conv: %s user_name: %s",
           purple_conversation_get_name (conv), name);

  treemodel = gtk_tree_view_get_model (GTK_TREE_VIEW(chatty_conv->muc.treeview));
  liststore = GTK_LIST_STORE(treemodel);

  account = purple_conversation_get_account (conv);

  prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

  if (prpl_info && prpl_info->get_cb_real_name) {
    chat_id = purple_conv_chat_get_id (PURPLE_CONV_CHAT(conv));

    real_who = prpl_info->get_cb_real_name (gc, chat_id, name);

    if (real_who) {
      buddy = purple_find_buddy (account, real_who);
      color = chatty_conv_muc_get_avatar_color (real_who);

      avatar = chatty_icon_get_buddy_icon ((PurpleBlistNode*)buddy,
                                           alias,
                                           CHATTY_ICON_SIZE_LARGE,
                                           color,
                                           FALSE);
    }
  }

  status = chatty_conv_muc_get_user_status (chat, name, flags);

  text = g_strconcat ("<span color='#646464'>",
                      name,
                      "</span>",
                      "\n",
                      "<small>",
                      status,
                      "</small>",
                      NULL);

  tmp = g_utf8_casefold (alias, -1);
  alias_key = g_utf8_collate_key (tmp, -1);
  g_free (tmp);

  gtk_list_store_insert_with_values (liststore, &iter,
                                     -1,
                                     MUC_COLUMN_AVATAR, avatar,
                                     MUC_COLUMN_ENTRY, text,
                                     MUC_COLUMN_NAME, name,
                                     MUC_COLUMN_ALIAS_KEY, alias_key,
                                     MUC_COLUMN_LAST, NULL,
                                     MUC_COLUMN_FLAGS, flags,
                                     -1);

  if (cb->ui_data) {
    GtkTreeRowReference *ref = cb->ui_data;
    gtk_tree_row_reference_free (ref);
  }

  path = gtk_tree_model_get_path (treemodel, &iter);
  cb->ui_data = gtk_tree_row_reference_new (treemodel, path);
  gtk_tree_path_free (path);

  if (avatar) {
    g_object_unref (avatar);
  }

  g_free (alias_key);
  g_free (real_who);
  g_free (status);
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
  PurpleConvChat     *chat;
  ChattyConversation *chatty_conv;
  GtkListStore       *ls;
  GList              *l;

  chat = PURPLE_CONV_CHAT(conv);
  chatty_conv = CHATTY_CONVERSATION(conv);

  chatty_conv->muc.user_count = g_list_length (purple_conv_chat_get_users (chat));

  ls = GTK_LIST_STORE(gtk_tree_view_get_model (GTK_TREE_VIEW(chatty_conv->muc.treeview)));

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE(ls),
                                        GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID,
                                        GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID);

  l = users;

  while (l != NULL) {
    chatty_conv_muc_add_user (conv, (PurpleConvChatBuddy *)l->data);
    l = l->next;
  }

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE(ls),
                                        MUC_COLUMN_ALIAS_KEY,
                                        GTK_SORT_ASCENDING);
}


/**
 * chatty_conv_muc_list_remove_users:
 * @conv:        a PurpleConversation
 * @users:       a Glist
 *
 * Remove users from the muc list
 *
 * invoked from PurpleConversationUiOps
 *
 */
static void
chatty_conv_muc_list_remove_users (PurpleConversation *conv,
                                   GList              *users)
{
  PurpleConvChat     *chat;
  ChattyConversation *chatty_conv;
  GtkTreeIter         iter;
  GtkTreeModel       *model;
  GList              *l;
  gboolean            result;

  chat = PURPLE_CONV_CHAT(conv);
  chatty_conv = CHATTY_CONVERSATION(conv);

  chatty_conv->muc.user_count = g_list_length (purple_conv_chat_get_users (chat));

  g_debug ("chatty_conv_muc_list_remove_users conv: %s user_count: %i",
           purple_conversation_get_name (conv),
           chatty_conv->muc.user_count);

  for (l = users; l != NULL; l = l->next) {
    model = gtk_tree_view_get_model (GTK_TREE_VIEW(chatty_conv->muc.treeview));

    if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL(model), &iter)) {
      continue;
    }

    do {
      char *val;

      gtk_tree_model_get (GTK_TREE_MODEL(model),
                          &iter,
                          MUC_COLUMN_NAME,
                          &val,
                          -1);

      if (!purple_utf8_strcasecmp ((char *)l->data, val)) {
        result = gtk_list_store_remove (GTK_LIST_STORE(model), &iter);
      } else {
        result = gtk_tree_model_iter_next (GTK_TREE_MODEL(model), &iter);
      }

      g_free (val);
    } while (result);
  }
}


/**
 * chatty_conv_muc_get_iter:
 * @cbuddy: a PurpleConvChatBuddy
 * @iter:   a GtkTreeIter
 *
 * Get muc list iter from chat buddy
 *
 * invoked from chatty_conv_muc_list_update_user
 *
 */
static gboolean chatty_conv_muc_get_iter (PurpleConvChatBuddy *cbuddy,
                                          GtkTreeIter         *iter)
{
  GtkTreeRowReference *ref;
  GtkTreePath         *path;
  GtkTreeModel        *model;

  g_return_val_if_fail (cbuddy != NULL, FALSE);

  ref = cbuddy->ui_data;

  if (!ref) {
    return FALSE;
  }

  if ((path = gtk_tree_row_reference_get_path (ref)) == NULL) {
    return FALSE;
  }

  model = gtk_tree_row_reference_get_model (ref);

  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL(model), iter, path)) {
    gtk_tree_path_free (path);

    return FALSE;
  }

  gtk_tree_path_free (path);

  return TRUE;
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
  PurpleConvChat      *chat;
  PurpleConvChatBuddy *cbuddy;
  ChattyConversation  *chatty_conv;
  GtkTreeIter          iter;
  GtkTreeModel        *model;

  chat = PURPLE_CONV_CHAT(conv);
  chatty_conv = CHATTY_CONVERSATION(conv);

  model = gtk_tree_view_get_model (GTK_TREE_VIEW(chatty_conv->muc.treeview));

  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL(model), &iter)) {
    return;
  }

  cbuddy = purple_conv_chat_cb_find (chat, user);

  if (!cbuddy) {
    return;
  } 

  g_debug ("chatty_conv_muc_list_update_user conv: %s user_name: %s",
           purple_conversation_get_name (conv), user);

  chatty_conv->muc.user_count = g_list_length (purple_conv_chat_get_users (chat));

  if (chatty_conv_muc_get_iter (cbuddy, &iter)) {
    GtkTreeRowReference *ref = cbuddy->ui_data;

    gtk_list_store_remove (GTK_LIST_STORE(model), &iter);
    gtk_tree_row_reference_free (ref);

    cbuddy->ui_data = NULL;
  }

  if (cbuddy) {
    chatty_conv_muc_add_user (conv, cbuddy);
  }
}


/**
 * chatty_conv_set_muc_topic:
 * @topic_text: a const char
 *
 * Update the muc topic text
 *
 * called from cb_button_edit_topic_clicked
 * in chatty-dialogs.c
 *
 */
void
chatty_conv_set_muc_topic (const char *topic_text)
{
  PurplePluginProtocolInfo *prpl_info = NULL;
  PurpleConnection         *gc;
  PurpleConversation       *conv;
  gint                      chat_id;

  chatty_data_t *chatty = chatty_get_data ();

  conv = chatty_conv_container_get_active_purple_conv (GTK_NOTEBOOK(chatty->pane_view_message_list));

  gc = purple_conversation_get_gc (conv);

  prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

  if (!gc || !prpl_info || !topic_text) {
    return;
  }

  if (prpl_info->set_chat_topic == NULL) {
    return;
  }

  chat_id = purple_conv_chat_get_id (PURPLE_CONV_CHAT(conv));

  prpl_info->set_chat_topic (gc, chat_id, topic_text);
}


/**
 * chatty_conv_set_muc_prefs:
 * @pref:  a gint
 * @value: a gboolean
 *
 * Set a muc preference
 *
 * called from cb_switch_prefs_state_changed
 * in chatty-dialogs.c
 *
 */
void
chatty_conv_set_muc_prefs (gint     pref,
                           gboolean value)
{
  PurpleBlistNode    *node;
  PurpleConnection   *gc;
  PurpleConversation *conv;

  chatty_data_t *chatty = chatty_get_data ();

  conv = chatty_conv_container_get_active_purple_conv (GTK_NOTEBOOK(chatty->pane_view_message_list));

  gc = purple_conversation_get_gc (conv);

  if (!gc || !pref) {
    return;
  }

  node = PURPLE_BLIST_NODE(purple_blist_find_chat (conv->account, conv->name));

  switch (pref) {
    case CHATTY_PREF_MUC_NOTIFICATIONS:
      purple_blist_node_set_bool (node, "chatty-notifications", value);
      break;
    case CHATTY_PREF_MUC_PERSISTANT:
      purple_blist_node_set_bool (node, "chatty-persistant", value);
      break;
    case CHATTY_PREF_MUC_AUTOJOIN:
      purple_blist_node_set_bool (node, "chatty-autojoin", value);
      break;
    default:
      break;
  }
}


/**
 * chatty_conv_update_muc_info:
 * @conv: a PurpleConversation
 *
 * Update the data in the muc info dialog
 *
 * called from chatty_conv_join_chat
 *
 */
static void
chatty_conv_update_muc_info (PurpleConversation *conv)
{
  ChattyConversation       *chatty_conv;
  PurplePluginProtocolInfo *prpl_info;
  PurpleConnection         *gc;
  PurpleBlistNode          *node;
  GtkWidget                *child;
  GList                    *children;
  char                     *user_count_str;
  const char               *chat_name;
  const char               *text;
  const char               *topic;

  chatty_data_t *chatty = chatty_get_data ();

  chatty_conv = CHATTY_CONVERSATION(conv);

  gc = purple_conversation_get_gc (conv);

  node = PURPLE_BLIST_NODE(purple_blist_find_chat (conv->account, conv->name));

  prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

  chat_name = purple_conversation_get_title (conv);

  text = _("members");
  user_count_str = g_strdup_printf ("%i %s", chatty_conv->muc.user_count, text);

  gtk_label_set_text (GTK_LABEL(chatty->muc.label_chat_id), chat_name);
  gtk_label_set_text (GTK_LABEL(chatty->muc.label_num_user), user_count_str);

  topic = purple_conv_chat_get_topic (PURPLE_CONV_CHAT(conv));
  topic = chatty_conv_check_for_links (topic);

  if (prpl_info->set_chat_topic == NULL) {
    gtk_label_set_text (GTK_LABEL(chatty->muc.label_topic), topic);

    gtk_widget_show (GTK_WIDGET(chatty->muc.label_topic));
    gtk_widget_hide (GTK_WIDGET(chatty->muc.box_topic_editor));
  } else {
    gtk_text_buffer_set_text (chatty->muc.msg_buffer_topic, topic, strlen (topic));

    gtk_widget_show (GTK_WIDGET(chatty->muc.box_topic_editor));
    gtk_widget_hide (GTK_WIDGET(chatty->muc.label_topic));
  }

  gtk_switch_set_state (chatty->muc.switch_prefs_notifications,
                        purple_blist_node_get_bool (node, "chatty-notifications"));

  gtk_switch_set_state (chatty->muc.switch_prefs_persistant,
                        purple_blist_node_get_bool (node, "chatty-persistant"));

  gtk_switch_set_state (chatty->muc.switch_prefs_autojoin,
                        purple_blist_node_get_bool (node, "chatty-autojoin"));

  children = gtk_container_get_children (GTK_CONTAINER(chatty->pane_view_muc_info));
  children = g_list_first (children);

  if (children != NULL) {
    child = children->data;

    g_object_ref (G_OBJECT(child));

    gtk_container_remove (GTK_CONTAINER(chatty->pane_view_muc_info),
                          GTK_WIDGET(child));
  }

  gtk_box_pack_start (GTK_BOX(chatty->pane_view_muc_info),
                      GTK_WIDGET(chatty_conv->muc.list),
                      TRUE, TRUE, 0);

  g_list_free (children);
  g_free (user_count_str);
}


/**
 * chatty_conv_invite_muc_user:
 * @user_name:  a const char
 * @invite_msg: a const char
 *
 * Invite a contact to a muc
 *
 * called from cb_button_invite_contact_clicked
 * in chatty-dialogs.c
 *
 */
void
chatty_conv_invite_muc_user (const char *user_name,
                             const char *invite_msg)
{
  PurpleConversation     *conv;
  PurpleConversationType  conv_type;

  chatty_data_t *chatty = chatty_get_data ();

  conv = chatty_conv_container_get_active_purple_conv (GTK_NOTEBOOK(chatty->pane_view_message_list));
  conv_type = purple_conversation_get_type (conv);

  if (conv && conv_type == PURPLE_CONV_TYPE_CHAT) {
    serv_chat_invite (purple_conversation_get_gc (conv),
                      purple_conv_chat_get_id (PURPLE_CONV_CHAT(conv)),
                      invite_msg,
                      user_name);
  }
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
  PurpleConversation      *conv = chatty_conv->conv;
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

  gtk_widget_grab_focus (chatty_conv->input.entry);
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
  PurpleConversationType    type;
  PurpleConnection         *gc;
  PurplePluginProtocolInfo *prpl_info = NULL;
  PurpleAccount            *account;
  PurpleBuddy              *buddy;
  PurpleBlistNode          *node;
  gchar                    *real_who = NULL;
  gchar                    *msg_html;
  const char               *color;
  gboolean                  group_chat;
  GdkPixbuf                *avatar = NULL;
  GtkWidget                *icon = NULL;
  int                       chat_id;
  const char               *conv_name;
  gchar                    *who_no_resource;
  gchar                    **line_split;

  chatty_conv = CHATTY_CONVERSATION (conv);

  g_return_if_fail (chatty_conv != NULL);

  if ((flags & PURPLE_MESSAGE_SYSTEM) && !(flags & PURPLE_MESSAGE_NOTIFY)) {
    flags &= ~(PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV);
  }

  conv_name = purple_conversation_get_name (chatty_conv->conv);

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

      real_who = prpl_info->get_cb_real_name(gc, chat_id, who);

      if (real_who) {
        buddy = purple_find_buddy (account, real_who);
        color = chatty_conv_muc_get_avatar_color (real_who);

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
  }

  if (*message != '\0') {

    line_split = g_strsplit ((const gchar*)who, "/", -1);
    who_no_resource = g_strdup(line_split[0]);

    if (flags & PURPLE_MESSAGE_RECV) {
      msg_html = chatty_conv_check_for_links (message);

      chatty_msg_list_add_message (chatty_conv->msg_list,
                                   MSG_IS_INCOMING,
                                   msg_html,
                                   group_chat ? who : NULL,
                                   icon ? icon : NULL);
      if (type == PURPLE_CONV_TYPE_CHAT){
        chatty_history_add_chat_message (message, 1, account->username, real_who, "UID", mtime, conv_name);  // TODO: LELAND: UID to be implemented by XEP-0313
      } else {
        chatty_history_add_im_message (message, 1, account->username, who_no_resource, "UID", mtime);
      }
      g_free (msg_html);
    } else if (flags & PURPLE_MESSAGE_SEND) {
      msg_html = chatty_conv_check_for_links (message);

      chatty_msg_list_add_message (chatty_conv->msg_list,
                                   MSG_IS_OUTGOING,
                                   msg_html,
                                   NULL,
                                   NULL);
      if (type == PURPLE_CONV_TYPE_CHAT){
        chatty_history_add_chat_message (message, -1, account->username, real_who, "UID", mtime, conv_name);
      } else {
        chatty_history_add_im_message (message, -1, account->username, who_no_resource, "UID", mtime);
      }
      g_free (msg_html);
    } else if (flags & PURPLE_MESSAGE_SYSTEM) {
      chatty_msg_list_add_message (chatty_conv->msg_list,
                                   MSG_IS_SYSTEM,
                                   message,
                                   NULL,
                                   NULL);
      if (type == PURPLE_CONV_TYPE_CHAT){
        // TODO: LELAND: Do not store "The topic is:" or store them but dont show to the user
        //chatty_history_add_chat_message (chat_id, message, 0, real_who, alias, "UID", mtime, conv_name);
      } else {
        //chatty_history_add_im_message (message, 0, account->username, who, "UID", mtime);
      }
    }

    g_free(who_no_resource);
    g_free(line_split);

    chatty_conv_set_unseen (chatty_conv, CHATTY_UNSEEN_NONE);
  }

  g_free (real_who);

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
  gint page_num;

  chatty_data_t *chatty = chatty_get_data();

  page_num = gtk_notebook_page_num (GTK_NOTEBOOK(chatty->pane_view_message_list),
                                    chatty_conv->tab_cont);

  gtk_notebook_set_current_page (GTK_NOTEBOOK(chatty->pane_view_message_list),
                                 page_num);

  g_debug ("chatty_conv_switch_conv active_conv: %s   page_num %i",
           purple_conversation_get_name (chatty_conv->conv), page_num);

  gtk_widget_grab_focus (GTK_WIDGET(chatty_conv->input.entry));
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
  guint index;

  chatty_data_t *chatty = chatty_get_data();

  index = gtk_notebook_page_num (GTK_NOTEBOOK(chatty->pane_view_message_list),
                                 chatty_conv->tab_cont);

  gtk_widget_destroy (GTK_WIDGET(chatty_conv->tab_cont));

  gtk_notebook_remove_page (GTK_NOTEBOOK(chatty->pane_view_message_list), index);

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
  ChattyConversation *chatty_conv;

  g_return_if_fail (account != NULL);
  g_return_if_fail (name != NULL);

  conv = purple_find_conversation_with_account (PURPLE_CONV_TYPE_IM,
                                                name,
                                                account);

  if (conv == NULL) {
    conv = purple_conversation_new (PURPLE_CONV_TYPE_IM,
                                    account,
                                    name);
  }

  purple_signal_emit (chatty_conversations_get_handle (),
                      "conversation-displayed",
                      CHATTY_CONVERSATION (conv));

  chatty_conv_present_conversation (conv);

  chatty_conv = CHATTY_CONVERSATION (conv);

  chatty_conv_set_unseen (chatty_conv, CHATTY_UNSEEN_NONE);
}

// TODO: LELAND: Should this go to some utilities file?
void
chatty_conv_add_history_since_component(GHashTable *components,
                                        const char *account,
                                        const char *room){
  time_t mtime;
  struct tm * timeinfo;
  g_autofree gchar *iso_timestamp = g_malloc0(MAX_GMT_ISO_SIZE * sizeof(char));
                               // TODO @LELAND for some reason if freed here,
                               // writes garbage to blist.xml
                               // Why cant I use an string?

  mtime = chatty_history_get_chat_last_message_time(account, room);
  mtime += 1; // Use the next epoch to exclude the last stored message(s)
  timeinfo = gmtime (&mtime);
  g_return_if_fail (strftime (iso_timestamp,
                              MAX_GMT_ISO_SIZE*sizeof(char),
                              "%Y-%m-%dT%H:%M:%SZ",
                              timeinfo));

  g_hash_table_steal (components, "history_since");
  g_hash_table_insert(components, "history_since", g_steal_pointer (&iso_timestamp));
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

    purple_signal_emit (chatty_conversations_get_handle (),
                        "conversation-displayed",
                        CHATTY_CONVERSATION (conv));

    chatty_conv = CHATTY_CONVERSATION (conv);

    chatty_conv_update_muc_info (conv);

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
  PurpleConversationType  type;

  GtkBuilder      *builder;
  GtkWidget       *scrolled;
  GtkAdjustment   *vadjust;
  GtkWidget       *msg_view_box;
  GtkWidget       *msg_view_list;
  GtkStyleContext *sc;

  gtk_icon_theme_add_resource_path (gtk_icon_theme_get_default (),
                                    "/sm/puri/chatty/icons/ui/");

  builder = gtk_builder_new_from_resource ("/sm/puri/chatty/ui/chatty-pane-msg-view.ui");

  chatty_conv->input.entry = GTK_WIDGET(gtk_builder_get_object (builder, "text_input"));

  msg_view_box = GTK_WIDGET(gtk_builder_get_object (builder, "msg_view_box"));
  msg_view_list = GTK_WIDGET(gtk_builder_get_object (builder, "msg_view_list"));
  scrolled = GTK_WIDGET(gtk_builder_get_object (builder, "scrolled"));
  chatty_conv->input.frame = GTK_WIDGET(gtk_builder_get_object (builder, "frame"));
  chatty_conv->input.button_send = GTK_WIDGET(gtk_builder_get_object (builder, "button_send"));
  chatty_conv->omemo.symbol_encrypt = GTK_IMAGE(gtk_builder_get_object (builder, "symbol_encrypt"));

  type = purple_conversation_get_type (chatty_conv->conv);

  if (type == PURPLE_CONV_TYPE_IM) {
    gtk_widget_show (GTK_WIDGET(chatty_conv->omemo.symbol_encrypt));
    chatty_lurch_get_status (chatty_conv->conv);
  }

  vadjust = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW(scrolled));

  g_signal_connect_after (G_OBJECT (vadjust),
                          "notify::upper",
                          G_CALLBACK (cb_msg_input_vadjust),
                          (gpointer) chatty_conv);

  chatty_conv->input.scrolled = scrolled;

  chatty_conv->input.buffer = gtk_text_buffer_new (NULL);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW(chatty_conv->input.entry),
                            chatty_conv->input.buffer);

  g_object_set_data (G_OBJECT(chatty_conv->input.buffer),
                              "user_data", chatty_conv);

  g_signal_connect (G_OBJECT(chatty_conv->input.entry),
                    "key-press-event",
                    G_CALLBACK(cb_textview_key_pressed),
                    (gpointer) chatty_conv);

  g_signal_connect (G_OBJECT(chatty_conv->input.entry),
                    "key-release-event",
                    G_CALLBACK(cb_textview_key_released),
                    (gpointer) chatty_conv);

  g_signal_connect (G_OBJECT(chatty_conv->input.entry),
                    "focus-in-event",
                    G_CALLBACK(cb_textview_focus_in),
                    (gpointer) chatty_conv->input.frame);

  g_signal_connect (G_OBJECT(chatty_conv->input.entry),
                    "focus-out-event",
                    G_CALLBACK(cb_textview_focus_out),
                    (gpointer) chatty_conv->input.frame);

  sc = gtk_widget_get_style_context (chatty_conv->input.button_send);

  switch (msg_type) {
    case CHATTY_MSG_TYPE_SMS:
      gtk_style_context_add_class (sc, "button_send_green");
      break;
    case CHATTY_MSG_TYPE_IM:
    case CHATTY_MSG_TYPE_IM_E2EE:
      gtk_style_context_add_class (sc, "suggested-action");
      break;
    case CHATTY_MSG_TYPE_UNKNOWN:
      break;
    default:
      break;
  }

  sc = gtk_widget_get_style_context (chatty_conv->input.frame);

  gtk_style_context_add_class (sc, "msg_entry_defocused");

  g_signal_connect (chatty_conv->input.button_send,
                    "clicked",
                    G_CALLBACK(cb_button_send_clicked),
                    (gpointer) chatty_conv);

  chatty_conv->msg_list = CHATTY_MSG_LIST (chatty_msg_list_new (msg_type, FALSE));

  g_signal_connect (chatty_conv->msg_list,
                    "message-added",
                    G_CALLBACK(cb_msg_list_message_added),
                    (gpointer) chatty_conv);

  gtk_box_pack_start (GTK_BOX (msg_view_list),
                      GTK_WIDGET (chatty_conv->msg_list),
                      TRUE, TRUE, 0);

  gtk_widget_show_all (msg_view_box);

  return msg_view_box;
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
    return;
  }

  chatty_conv = g_new0 (ChattyConversation, 1);
  conv->ui_data = chatty_conv;
  chatty_conv->conv = conv;

  account = purple_conversation_get_account (conv);
  protocol_id = purple_account_get_protocol_id (account);

  if (conv_type == PURPLE_CONV_TYPE_IM || conv_type == PURPLE_CONV_TYPE_CHAT) {
    chatty_conv->conv_header = g_malloc0 (sizeof (ChattyConvViewHeader));
  }

  if (conv_type == PURPLE_CONV_TYPE_CHAT) {
    chatty_conv_create_muc_list (chatty_conv);

    msg_type = CHATTY_MSG_TYPE_MUC;
  } else if (conv_type == PURPLE_CONV_TYPE_IM) {
    // Add SMS and IMs from unknown contacts to the chats-list,
    // but do not add them to the contacts-list and in case of
    // instant messages do not sync contacts with the server
    conv_name = purple_conversation_get_name (conv);
    buddy = purple_find_buddy (account, conv_name);

    if (buddy == NULL) {
      buddy = purple_buddy_new (account, conv_name, NULL);
      purple_blist_add_buddy (buddy, NULL, NULL, NULL);
      // flag the node in the blist so it can be set off in the chats-list
      purple_blist_node_set_bool (PURPLE_BLIST_NODE(buddy), "chatty-unknown-contact", TRUE);

      g_debug ("Unknown contact %s added to blist", purple_buddy_get_name (buddy));
    }

    if (g_strcmp0 (protocol_id, "prpl-mm-sms") == 0) {
      msg_type = CHATTY_MSG_TYPE_SMS;
    } else {
      msg_type = CHATTY_MSG_TYPE_IM;
    }
  }

  chatty_conv->tab_cont = chatty_conv_setup_pane (chatty_conv, msg_type);
  g_object_set_data (G_OBJECT(chatty_conv->tab_cont),
                     "ChattyConversation",
                     chatty_conv);

  gtk_widget_hide (chatty_conv->input.button_send);
  gtk_widget_show (chatty_conv->tab_cont);

  if (chatty_conv->tab_cont == NULL) {
    if (conv_type == PURPLE_CONV_TYPE_IM || conv_type == PURPLE_CONV_TYPE_CHAT) {
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

  chatty_conv_add_message_history_to_conv(chatty_conv);

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
  chatty_conv_muc_list_remove_users,
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
 * chatty_conv_container_init:
 *
 * Sets the notebook container for the
 * conversation panes
 *
 * called from chatty_window_init_data()
 *
 */
void
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
  g_hash_table_destroy (ht_sms_id);
  purple_prefs_disconnect_by_handle (chatty_conversations_get_handle());
  purple_signals_disconnect_by_handle (chatty_conversations_get_handle());
  purple_signals_unregister_by_instance (chatty_conversations_get_handle());
}
