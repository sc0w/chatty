/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#define _GNU_SOURCE
#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <purple.h>
#include "chatty-config.h"
#include "chatty-window.h"
#include "chatty-contact-row.h"
#include "chatty-folks.h"
#include "chatty-manager.h"
#include "chatty-settings.h"
#include "chatty-message-list.h"
#include "chatty-buddy-list.h"
#include "chatty-conversation.h"
#include "chatty-manager.h"
#include "chatty-purple-init.h"
#include "chatty-icons.h"
#include "dialogs/chatty-settings-dialog.h"
#include "dialogs/chatty-new-chat-dialog.h"
#include "dialogs/chatty-new-muc-dialog.h"
#include "dialogs/chatty-user-info-dialog.h"
#include "dialogs/chatty-muc-info-dialog.h"


struct _ChattyWindow
{
  GtkApplicationWindow parent_instance;

  ChattySettings *settings;

  GtkWidget *chats_listbox;

  GtkWidget *content_box;
  GtkWidget *header_box;
  GtkWidget *header_group;

  GtkWidget *sub_header_icon;
  GtkWidget *sub_header_label;

  GtkWidget *new_chat_dialog;

  GtkWidget *chats_search_bar;
  GtkWidget *chats_search_entry;

  GtkWidget *menu_add_contact_button;
  GtkWidget *menu_add_in_contacts_button;
  GtkWidget *menu_new_group_chat_button;
  GtkWidget *header_chat_info_button;
  GtkWidget *header_add_chat_button;
  GtkWidget *header_sub_menu_button;

  GtkWidget *convs_notebook;

  GtkWidget *overlay;
  GtkWidget *overlay_icon;
  GtkWidget *overlay_label_1;
  GtkWidget *overlay_label_2;
  GtkWidget *overlay_label_3;

  ChattyManager *manager;
  char      *uri;
  
  gboolean daemon_mode;
  gboolean im_account_connected;
  gboolean sms_account_connected;
};


G_DEFINE_TYPE (ChattyWindow, chatty_window, GTK_TYPE_APPLICATION_WINDOW)


static void chatty_update_header (ChattyWindow *self);


enum {
  PROP_0,
  PROP_DAEMON,
  PROP_SETTINGS,
  PROP_URI,
  PROP_LAST
};

static GParamSpec *props[PROP_LAST];


typedef struct {
  const char *title;
  const char *text_1;
  const char *text_2;
  const char *icon_name;
  int         icon_size;
} overlay_content_t;

overlay_content_t OverlayContent[6] = {
  {.title  = N_("Choose a contact"),
   .text_1 = N_("Select an <b>SMS</b> or <b>Instant Message</b> "
                "contact with the <b>\"+\"</b> button in the titlebar."),
   .text_2 = NULL,
  },
  {.title  = N_("Choose a contact"),
   .text_1 = N_("Select an <b>Instant Message</b> contact with "
                "the \"+\" button in the titlebar."),
   .text_2 = NULL,
  },
  {.title  = N_("Choose a contact"),
   .text_1 = N_("Start a <b>SMS</b> chat with with the \"+\" button in the titlebar."),
   .text_2 = N_("For <b>Instant Messaging</b> add or activate "
                "an account in <i>\"preferences\"</i>."),
  },
  {.title  = N_("Start chatting"),
   .text_1 = N_("For <b>Instant Messaging</b> add or activate "
                "an account in <i>\"preferences\"</i>."),
   .text_2 = NULL,
  }
};

static gint
chatty_blist_sort (GtkListBoxRow *row1,
                   GtkListBoxRow *row2,
                   gpointer       user_data)
{
  PurpleBlistNode *node1;
  ChattyBlistNode *chatty_node1;
  PurpleBlistNode *node2;
  ChattyBlistNode *chatty_node2;

  g_object_get (row1, "data", &node1, NULL);
  chatty_node1 = node1->ui_data;

  g_object_get (row2, "data", &node2, NULL);
  chatty_node2 = node2->ui_data;

  if (chatty_node1 != NULL && chatty_node2 != NULL)
    return difftime (chatty_node2->conv.last_msg_ts_raw, chatty_node1->conv.last_msg_ts_raw);

  return 0;
}


static gboolean
filter_chat_list_cb (GtkListBoxRow *row,
                     ChattyWindow  *self)
{
  const gchar *query;
  g_autofree gchar *name = NULL;

  g_assert (CHATTY_IS_CONTACT_ROW (row));
  g_assert (CHATTY_IS_WINDOW (self));

  query = gtk_entry_get_text (GTK_ENTRY (self->chats_search_entry));

  g_object_get (row, "name", &name, NULL);

  return ((*query == '\0') || (name && strcasestr (name, query)));
}


static void
window_chat_row_activated_cb (GtkListBox    *box,
                              GtkListBoxRow *row,
                              ChattyWindow  *self)
{
  ChattyWindow    *window;
  PurpleBlistNode *node;
  PurpleAccount   *account;
  PurpleChat      *chat;
  GdkPixbuf       *avatar;
  const char      *chat_name;
  const char      *number;

  g_assert (CHATTY_WINDOW (self));

  window = self;

  g_object_get (row, "phone_number", &number, NULL);

  if (number != NULL) {
    chatty_blist_add_buddy_from_uri (number);

    return;
  }

  g_object_get (row, "data", &node, NULL);

  chatty_window_set_menu_add_contact_button_visible (self, FALSE);
  chatty_window_set_menu_add_in_contacts_button_visible (self, FALSE);

  if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
    PurpleBuddy *buddy;

    buddy = (PurpleBuddy*)node;
    account = purple_buddy_get_account (buddy);

    chatty_window_set_header_chat_info_button_visible (window, FALSE);

    if (chatty_blist_protocol_is_sms (account)) {
      ChattyEds *chatty_eds;
      ChattyContact *contact;

      chatty_eds = chatty_manager_get_eds (chatty_manager_get_default ());
      number = purple_buddy_get_name (buddy);
      contact = chatty_eds_find_by_number (chatty_eds, number);

      if (!contact) {
        chatty_window_set_menu_add_in_contacts_button_visible (window, TRUE);
      }
    }

    if (purple_blist_node_get_bool (PURPLE_BLIST_NODE(buddy),
                                    "chatty-unknown-contact")) {

      chatty_window_set_menu_add_contact_button_visible (window, TRUE);
    }

    purple_blist_node_set_bool (node, "chatty-autojoin", TRUE);

    chatty_conv_im_with_buddy (account, purple_buddy_get_name (buddy));

    chatty_window_set_new_chat_dialog_visible (window, FALSE);

  } else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
    chat = (PurpleChat*)node;
    chat_name = purple_chat_get_name (chat);

    chatty_conv_join_chat (chat);

    purple_blist_node_set_bool (node, "chatty-autojoin", TRUE);

    avatar = chatty_icon_get_buddy_icon (node,
                                         NULL,
                                         CHATTY_ICON_SIZE_SMALL,
                                         CHATTY_COLOR_GREY,
                                         FALSE);

    chatty_window_update_sub_header_titlebar (window, avatar, chat_name);
    chatty_window_change_view (window, CHATTY_VIEW_MESSAGE_LIST);

    chatty_window_set_new_chat_dialog_visible (window, FALSE);

    g_object_unref (avatar);
  }
}


static void
window_chat_changed_cb (ChattyWindow *self)
{
  gboolean has_child;

  g_assert (CHATTY_IS_WINDOW (self));

  has_child = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->chats_listbox), 0) != NULL;
  chatty_window_set_overlay_visible (self, !has_child);
  gtk_widget_set_sensitive (self->header_sub_menu_button, has_child);
}

static void
header_visible_child_cb (GObject      *sender,
                         GParamSpec   *pspec,
                         ChattyWindow *self)
{
  g_assert (CHATTY_IS_WINDOW (self));

  chatty_update_header (self);
}


static void
notify_fold_cb (GObject      *sender,
                GParamSpec   *pspec,
                ChattyWindow *self)
{
  HdyFold fold = hdy_leaflet_get_fold (HDY_LEAFLET (self->header_box));

  if (fold != HDY_FOLD_FOLDED)
    chatty_blist_chat_list_select_first ();

  chatty_update_header (self);
}


static void
chatty_update_header (ChattyWindow *self)
{
  GtkWidget *header_child = hdy_leaflet_get_visible_child (HDY_LEAFLET (self->header_box));
  HdyFold fold = hdy_leaflet_get_fold (HDY_LEAFLET (self->header_box));

  g_assert (CHATTY_IS_WINDOW (self));
  g_assert (header_child == NULL || GTK_IS_HEADER_BAR (header_child));

  hdy_header_group_set_focus (HDY_HEADER_GROUP (self->header_group), 
                              fold == HDY_FOLD_FOLDED ? 
                              GTK_HEADER_BAR (header_child) : NULL);
}


static void
msg_view_delete_action (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{
  chatty_blist_chat_list_remove_buddy ();
}


static void
msg_view_leave_action (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  chatty_blist_chat_list_leave_chat ();
}

static void
msg_view_add_contact_action (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  chatty_blist_contact_list_add_buddy ();
}

static void
msg_view_add_in_contacts_action (GSimpleAction *action,
                                 GVariant      *parameter,
                                 gpointer       user_data)
{
  chatty_blist_gnome_contacts_add_buddy ();
}

static void
msg_view_chat_info_action (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
  ChattyWindow *self = user_data;

  chatty_window_change_view (self, CHATTY_VIEW_CHAT_INFO);
}


static void
chatty_add_contact_action (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
  ChattyWindow *self = user_data;

  chatty_window_change_view (self, CHATTY_VIEW_NEW_CHAT);
}


static void
chatty_back_action (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  ChattyWindow *self = user_data;

  /*
   * Clears 'selected_node' which is evaluated to
   * block the counting of pending messages
   * while chatting with this node
   */
  gtk_list_box_unselect_all (GTK_LIST_BOX (self->chats_listbox));
  chatty_window_change_view (self, CHATTY_VIEW_CHAT_LIST);
}


static void
chatty_new_chat_action (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{
  ChattyWindow *self = user_data;

  chatty_window_change_view (self, CHATTY_VIEW_NEW_CHAT);
}


static void
chatty_window_show_settings_dialog (ChattyWindow *self)
{
  GtkWidget *dialog;

  g_assert (CHATTY_IS_WINDOW (self));

  dialog = chatty_settings_dialog_new (GTK_WINDOW (self));
  gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);
}


static void
chatty_window_show_new_muc_dialog (ChattyWindow *self)
{
  GtkWidget *dialog;

  g_assert (CHATTY_IS_WINDOW (self));

  dialog = chatty_new_muc_dialog_new (GTK_WINDOW (self));
  gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);
}


static GtkWidget *
chatty_window_create_new_chat_dialog (ChattyWindow *self)
{
  GtkWidget *dialog;

  g_assert (CHATTY_IS_WINDOW (self));

  dialog = chatty_new_chat_dialog_new (GTK_WINDOW (self));

  return dialog;
}


static void
chatty_window_show_chat_info (ChattyWindow *self)
{
  GtkWidget *dialog;

  ChattyConversation *chatty_conv;

  g_assert (CHATTY_IS_WINDOW (self));

  chatty_conv = chatty_conv_container_get_active_chatty_conv (GTK_NOTEBOOK (self->convs_notebook));

  if (purple_conversation_get_type (chatty_conv->conv) == PURPLE_CONV_TYPE_IM)
    dialog = chatty_user_info_dialog_new (GTK_WINDOW (self), (gpointer)chatty_conv);
  else if (purple_conversation_get_type (chatty_conv->conv) == PURPLE_CONV_TYPE_CHAT)
    dialog = chatty_muc_info_dialog_new (GTK_WINDOW (self), (gpointer)chatty_conv);

  gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);
}


/* Copied from chatty-dialogs.c written by Andrea Schäfer <mosibasu@me.com> */
static void
chatty_window_show_about_dialog (ChattyWindow *self)
{
  static const gchar *authors[] = {
    "Adrien Plazas <kekun.plazas@laposte.net>",
    "Andrea Schäfer <mosibasu@me.com>",
    "Benedikt Wildenhain <benedikt.wildenhain@hs-bochum.de>",
    "Guido Günther <agx@sigxcpu.org>",
    "Julian Sparber <jsparber@gnome.org>",
    "Leland Carlye <leland.carlye@protonmail.com>",
    "Mohammed Sadiq https://www.sadiqpk.org/",
    "Richard Bayerle (OMEMO Plugin) https://github.com/gkdr/lurch",
    "Ruslan Marchenko <me@ruff.mobi>",
    "and more...",
    NULL
  };

  static const gchar *artists[] = {
    "Tobias Bernard <tbernard@gnome.org>",
    NULL
  };

  static const gchar *documenters[] = {
    "Heather Ellsworth <heather.ellsworth@puri.sm>",
    NULL
  };

  gtk_show_about_dialog (GTK_WINDOW (self),
                         "logo-icon-name", CHATTY_APP_ID,
                         "program-name", _("Chats"),
                         "version", GIT_VERSION,
                         "comments", _("An SMS and XMPP messaging client"),
                         "website", "https://source.puri.sm/Librem5/chatty",
                         "copyright", "© 2018 Purism SPC",
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "authors", authors,
                         "artists", artists,
                         "documenters", documenters,
                         "translator-credits", _("translator-credits"),
                         NULL);
}


void
chatty_window_change_view (ChattyWindow      *self,
                           ChattyWindowState  view)
{
  g_assert (CHATTY_IS_WINDOW (self));

  switch (view) {
    case CHATTY_VIEW_SETTINGS:
      chatty_window_show_settings_dialog (self);
      break;
    case CHATTY_VIEW_ABOUT_CHATTY:
      chatty_window_show_about_dialog (self);
      break;
    case CHATTY_VIEW_JOIN_CHAT:
      chatty_window_show_new_muc_dialog (self);
      break;
    case CHATTY_VIEW_NEW_CHAT:
      gtk_widget_show (GTK_WIDGET (self->new_chat_dialog));
      break;
    case CHATTY_VIEW_CHAT_INFO:
      chatty_window_show_chat_info (self);
      break;
    case CHATTY_VIEW_MESSAGE_LIST:
      hdy_leaflet_set_visible_child_name (HDY_LEAFLET (self->content_box), "content");
      break;
    case CHATTY_VIEW_CHAT_LIST:
      hdy_leaflet_set_visible_child_name (HDY_LEAFLET (self->content_box), "sidebar");
      break;
    default:
      ;
  }
}


void
chatty_window_update_sub_header_titlebar (ChattyWindow *self,
                                          GdkPixbuf    *icon,
                                          const char   *title)
{
  g_assert (CHATTY_IS_WINDOW (self));

  if (icon != NULL)
    gtk_image_set_from_pixbuf (GTK_IMAGE (self->sub_header_icon), icon);
  else
    gtk_image_clear (GTK_IMAGE (self->sub_header_icon));

  gtk_label_set_label (GTK_LABEL (self->sub_header_label), title);
}


void
chatty_window_set_overlay_visible (ChattyWindow *self,
                                   gboolean      visible)
{
  gint   mode;
  guint8 accounts = 0;

  g_assert (CHATTY_IS_WINDOW (self));

  if (visible) {
    gtk_widget_show (GTK_WIDGET (self->overlay));
  } else {
    gtk_widget_hide (GTK_WIDGET (self->overlay));
    return;
  }

  if (self->sms_account_connected)
    accounts |= CHATTY_ACCOUNTS_SMS;

  if (self->im_account_connected )
    accounts |= CHATTY_ACCOUNTS_IM;

  if (accounts == CHATTY_ACCOUNTS_IM_SMS)
    mode = CHATTY_OVERLAY_EMPTY_CHAT;
  else if (accounts == CHATTY_ACCOUNTS_SMS)
    mode = CHATTY_OVERLAY_EMPTY_CHAT_NO_IM;
  else if (accounts == CHATTY_ACCOUNTS_IM)
    mode = CHATTY_OVERLAY_EMPTY_CHAT_NO_SMS;
  else
    mode = CHATTY_OVERLAY_EMPTY_CHAT_NO_SMS_IM;

  gtk_image_set_from_icon_name (GTK_IMAGE (self->overlay_icon),
                                "sm.puri.Chatty-symbolic",
                                0);

  gtk_image_set_pixel_size (GTK_IMAGE (self->overlay_icon), 96);

  gtk_label_set_text (GTK_LABEL (self->overlay_label_1),
                      gettext (OverlayContent[mode].title));
  gtk_label_set_text (GTK_LABEL (self->overlay_label_2),
                      gettext (OverlayContent[mode].text_1));
  gtk_label_set_text (GTK_LABEL (self->overlay_label_3),
                      gettext (OverlayContent[mode].text_2));

  gtk_label_set_use_markup (GTK_LABEL (self->overlay_label_1), TRUE);
  gtk_label_set_use_markup (GTK_LABEL (self->overlay_label_2), TRUE);
  gtk_label_set_use_markup (GTK_LABEL (self->overlay_label_3), TRUE);
}


static int
window_authorize_buddy_cb (ChattyWindow    *self,
                           ChattyPpAccount *account,
                           const char      *remote_user,
                           const char      *name)
{
  GtkWidget *dialog;
  GtkWindow *window;
  int response;

  g_assert (CHATTY_IS_WINDOW (self));
  g_assert (CHATTY_IS_PP_ACCOUNT (account));

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
  dialog = gtk_message_dialog_new (window,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("Authorize %s?"),
                                   name);

  gtk_dialog_add_buttons (GTK_DIALOG(dialog),
                          _("Reject"),
                          GTK_RESPONSE_REJECT,
                          _("Accept"),
                          GTK_RESPONSE_ACCEPT,
                          NULL);

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("Add %s to contact list"),
                                            remote_user);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);

  response = gtk_dialog_run (GTK_DIALOG(dialog));

  gtk_widget_destroy (dialog);

  return response;
}

static void
window_buddy_added_cb (ChattyWindow    *self,
                       ChattyPpAccount *account,
                       const char      *remote_user,
                       const char      *id)
{
  GtkWindow *window;
  GtkWidget *dialog;

  g_assert (CHATTY_IS_WINDOW (self));
  g_assert (CHATTY_IS_ACCOUNT (account));

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
  dialog = gtk_message_dialog_new (window,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_OK,
                                   _("Contact added"));

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("User %s has added %s to the contacts"),
                                            remote_user, id);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);

  gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);
}

static void
window_active_protocols_changed_cb (ChattyWindow *self)
{
  ChattyProtocol protocols;
  gboolean has_sms, has_im;

  g_assert (CHATTY_IS_WINDOW (self));

  protocols = chatty_manager_get_active_protocols (self->manager);
  has_sms = !!(protocols & CHATTY_PROTOCOL_SMS);
  has_im  = !!(protocols & ~CHATTY_PROTOCOL_SMS);

  gtk_widget_set_sensitive (self->header_add_chat_button, has_sms || has_im);
  gtk_widget_set_sensitive (self->menu_new_group_chat_button, has_im);
  window_chat_changed_cb (self);
}


static void
window_show_connection_error (ChattyWindow    *self,
                              ChattyPpAccount *account,
                              const char      *message)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (NULL,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_OK,
                                   _("Login failed"));

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG(dialog),
                                            "%s: %s\n\n%s",
                                            message,
                                            chatty_pp_account_get_username (account),
                                            _("Please check ID and password"));

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
  gtk_window_set_position (GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

  gtk_dialog_run (GTK_DIALOG(dialog));

  gtk_widget_destroy (dialog);
}


static void
chatty_window_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  ChattyWindow *self = (ChattyWindow *)object;

  switch (prop_id) {
    case PROP_DAEMON:
      self->daemon_mode = g_value_get_boolean (value);
      break;

    case PROP_SETTINGS:
      self->settings = g_value_dup_object (value);
      break;

    case PROP_URI:
      g_free (self->uri);
      self->uri = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}


static void
chatty_window_unmap (GtkWidget *widget)
{
  ChattyWindow *self = (ChattyWindow *)widget;
  GtkWindow    *window = (GtkWindow *)widget;
  GdkRectangle  geometry;
  gboolean      is_maximized;

  is_maximized = gtk_window_is_maximized (window);

  chatty_settings_set_window_maximized (self->settings, is_maximized);

  if (!is_maximized) {
    gtk_window_get_size (window, &geometry.width, &geometry.height);
    chatty_settings_set_window_geometry (self->settings, &geometry);
  }

  GTK_WIDGET_CLASS (chatty_window_parent_class)->unmap (widget);
}


static void
chatty_window_constructed (GObject *object)
{
  ChattyWindow *self = (ChattyWindow *)object;
  GtkWindow    *window = (GtkWindow *)object;
  GdkRectangle  geometry;

  GSimpleActionGroup *simple_action_group;

  const GActionEntry window_action_entries [] = {
    { "add", chatty_new_chat_action },
    { "add-contact", chatty_add_contact_action },
    { "back", chatty_back_action },
  };


  const GActionEntry msg_view_entries [] =
  {
    { "add-contact", msg_view_add_contact_action },
    { "add-gnome-contact", msg_view_add_in_contacts_action },
    { "leave-chat", msg_view_leave_action },
    { "delete-chat", msg_view_delete_action },
    { "chat-info", msg_view_chat_info_action }
  };

  chatty_settings_get_window_geometry (self->settings, &geometry);
  gtk_window_set_default_size (window, geometry.width, geometry.height);

  if (chatty_settings_get_window_maximized (self->settings))
    gtk_window_maximize (window);

  self->new_chat_dialog = chatty_window_create_new_chat_dialog (self);

  if (self->daemon_mode)
    g_signal_connect (G_OBJECT (self),
                      "delete-event",
                      G_CALLBACK (gtk_widget_hide_on_delete),
                      NULL);

  hdy_leaflet_set_visible_child_name (HDY_LEAFLET (self->content_box), "sidebar");

  hdy_search_bar_connect_entry (HDY_SEARCH_BAR(self->chats_search_bar),
                                GTK_ENTRY (self->chats_search_entry));

  gtk_widget_set_sensitive (GTK_WIDGET (self->header_sub_menu_button), FALSE);

  simple_action_group = g_simple_action_group_new ();

  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   window_action_entries,
                                   G_N_ELEMENTS (window_action_entries),
                                   window);

  gtk_widget_insert_action_group (GTK_WIDGET (window),
                                  "win",
                                  G_ACTION_GROUP (simple_action_group));

  simple_action_group = g_simple_action_group_new ();

  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   msg_view_entries,
                                   G_N_ELEMENTS (msg_view_entries),
                                   window);

  gtk_widget_insert_action_group (GTK_WIDGET (window),
                                  "msg_view",
                                  G_ACTION_GROUP (simple_action_group));

  chatty_window_change_view (self, CHATTY_VIEW_CHAT_LIST);

  gtk_widget_set_sensitive (self->header_add_chat_button, FALSE);
  window_chat_changed_cb (self);

  G_OBJECT_CLASS (chatty_window_parent_class)->constructed (object);
}


static void
chatty_window_finalize (GObject *object)
{
  ChattyWindow *self = (ChattyWindow *)object;

  chatty_purple_quit ();

  g_object_unref (self->settings);
  g_object_unref (self->manager);

  g_free (self->uri);

  G_OBJECT_CLASS (chatty_window_parent_class)->finalize (object);
}


static void
chatty_window_class_init (ChattyWindowClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = chatty_window_set_property;
  object_class->constructed  = chatty_window_constructed;
  object_class->finalize     = chatty_window_finalize;

  widget_class->unmap = chatty_window_unmap;

  props[PROP_DAEMON] =
    g_param_spec_boolean ("daemon-mode",
                          "Daemon Mode",
                          "Application started in daemon mode",
                          FALSE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  props[PROP_SETTINGS] =
    g_param_spec_object ("settings",
                         "Settings",
                         "Application settings",
                         CHATTY_TYPE_SETTINGS,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_URI] =
    g_param_spec_string ("uri",
                         "An URI",
                         "An URI string passed to the application",
                         "",
                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST, props);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/chatty/"
                                               "ui/chatty-window.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, sub_header_label);
  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, sub_header_icon);
  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, menu_add_contact_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, menu_add_in_contacts_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, menu_new_group_chat_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, header_chat_info_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, header_add_chat_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, header_sub_menu_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, chats_search_bar);
  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, chats_search_entry);
  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, content_box);
  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, header_box);
  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, header_group);
  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, chats_listbox);
  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, convs_notebook);
  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, overlay);
  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, overlay_icon);
  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, overlay_label_1);
  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, overlay_label_2);
  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, overlay_label_3);

  gtk_widget_class_bind_template_callback (widget_class, notify_fold_cb);
  gtk_widget_class_bind_template_callback (widget_class, header_visible_child_cb);
  gtk_widget_class_bind_template_callback (widget_class, window_chat_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, window_chat_changed_cb);
}


static void
chatty_window_init (ChattyWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->chats_listbox), chatty_blist_sort, NULL, NULL);
  gtk_list_box_set_filter_func (GTK_LIST_BOX (self->chats_listbox),
                                (GtkListBoxFilterFunc)filter_chat_list_cb, self, NULL);

  self->manager = g_object_ref (chatty_manager_get_default ());
  g_signal_connect_object (self->manager, "authorize-buddy",
                           G_CALLBACK (window_authorize_buddy_cb), self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager, "notify-added",
                           G_CALLBACK (window_buddy_added_cb), self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager, "notify::active-protocols",
                           G_CALLBACK (window_active_protocols_changed_cb), self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager, "connection-error",
                           G_CALLBACK (window_show_connection_error), self,
                           G_CONNECT_SWAPPED);
}


GtkWidget *
chatty_window_new (GtkApplication *application,
                   gboolean        daemon_mode,
                   ChattySettings *settings,
                   const char     *uri)
{
  g_assert (GTK_IS_APPLICATION (application));
  g_assert (CHATTY_IS_SETTINGS (settings));

  return g_object_new (CHATTY_TYPE_WINDOW,
                       "application", application,
                       "daemon-mode", daemon_mode,
                       "settings", settings,
                       "uri", uri,
                       NULL);
}


void 
chatty_window_set_new_chat_dialog_visible (ChattyWindow *self,
                                           gboolean      visible)
{
  gtk_widget_set_visible (self->new_chat_dialog, visible);
}


void 
chatty_window_set_menu_add_contact_button_visible (ChattyWindow *self,
                                                   gboolean      visible)
{
  gtk_widget_set_visible (self->menu_add_contact_button, visible);
}


void 
chatty_window_set_menu_add_in_contacts_button_visible (ChattyWindow *self,
                                                         gboolean      visible)
{
  gtk_widget_set_visible (self->menu_add_in_contacts_button, visible);
}


void 
chatty_window_set_header_chat_info_button_visible (ChattyWindow *self,
                                                   gboolean      visible)
{
  gtk_widget_set_visible (self->header_chat_info_button, visible);
}


const char *
chatty_window_get_uri (ChattyWindow *self)
{
  g_return_val_if_fail (CHATTY_IS_WINDOW (self), NULL);

  return self->uri;
}


GtkWidget *
chatty_window_get_chats_listbox (ChattyWindow *self)
{
  g_return_val_if_fail (CHATTY_IS_WINDOW (self), NULL);

  return self->chats_listbox;
}


GtkWidget *
chatty_window_get_convs_notebook (ChattyWindow *self)
{
  g_return_val_if_fail (CHATTY_IS_WINDOW (self), NULL);

  return self->convs_notebook;
}
