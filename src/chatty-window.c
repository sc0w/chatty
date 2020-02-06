/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include "chatty-config.h"
#include "chatty-window.h"
#include "chatty-settings.h"
#include "chatty-message-list.h"
#include "chatty-buddy-list.h"
#include "chatty-conversation.h"
#include "chatty-account.h"
#include "chatty-purple-init.h"
#include "chatty-icons.h"
#include "chatty-popover-actions.h"
#include "dialogs/chatty-dialogs.h"
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

  chatty_blist_returned_from_chat ();
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
      chatty_dialogs_show_dialog_about_chatty ();
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
      self->uri = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}


static void
chatty_window_constructed (GObject *object)
{
  ChattyWindow *self = (ChattyWindow *)object;
  GtkWindow    *window = (GtkWindow *)object;

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

  chatty_popover_actions_init (window);

  chatty_window_change_view (self, CHATTY_VIEW_CHAT_LIST);

  G_OBJECT_CLASS (chatty_window_parent_class)->constructed (object);
}


static void
chatty_window_dispose (GObject *object)
{
  ChattyWindow *self = (ChattyWindow *)object;

  if (self->chats_listbox)
    chatty_blist_disconnect_listbox (self->chats_listbox);

  G_OBJECT_CLASS (chatty_window_parent_class)->dispose (object);
}


static void
chatty_window_finalize (GObject *object)
{
  ChattyWindow *self = (ChattyWindow *)object;

  chatty_purple_quit ();

  g_object_unref (self->settings);

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
  object_class->dispose      = chatty_window_dispose;
  object_class->finalize     = chatty_window_finalize;

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
}


static void
chatty_window_init (ChattyWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
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


void
chatty_window_set_button_group_chat_sensitive (ChattyWindow *self,
                                               gboolean      sensitive)
{
  gtk_widget_set_sensitive (self->menu_new_group_chat_button, sensitive);
}


void
chatty_window_set_header_add_chat_button_sensitive (ChattyWindow *self,
                                                    gboolean      sensitive)
{
  gtk_widget_set_sensitive (self->header_add_chat_button, sensitive);
}


void
chatty_window_set_header_sub_menu_button_sensitive (ChattyWindow *self,
                                                    gboolean      sensitive)
{
  gtk_widget_set_sensitive (self->header_sub_menu_button, sensitive);
}


const char *
chatty_window_get_uri (ChattyWindow *self)
{
  g_return_val_if_fail (CHATTY_IS_WINDOW (self), NULL);

  return self->uri;
}


// TODO: Needs to be moved to accounts-manager
void
chatty_window_set_im_account_connected (ChattyWindow *self,
                                        gboolean      connected)
{
  self->im_account_connected = connected;
}


// TODO: Needs to be moved to accounts-manager
gboolean
chatty_window_get_im_account_connected (ChattyWindow *self)
{
  return self->im_account_connected;
}


// TODO: Needs to be moved to accounts-manager
void
chatty_window_set_sms_account_connected (ChattyWindow *self,
                                         gboolean      connected)
{
  self->sms_account_connected = connected;
}


// TODO: Needs to be moved to accounts-manager
gboolean
chatty_window_get_sms_account_connected (ChattyWindow *self)
{
  return self->sms_account_connected;
}


GtkWidget *
chatty_window_get_search_entry (ChattyWindow *self)
{
  g_return_val_if_fail (CHATTY_IS_WINDOW (self), NULL);

  return self->chats_search_entry;
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


GtkWidget *
chatty_window_get_new_chat_dialog (ChattyWindow *self)
{
  g_return_val_if_fail (CHATTY_IS_WINDOW (self), NULL);

  return self->new_chat_dialog;
}