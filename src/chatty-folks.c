/*
 * Copyright (C) 2019 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-folks"

#include <glib.h>
#include <gee-0.8/gee.h>
#include <folks/folks.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "purple.h"
#include "chatty-utils.h"
#include "chatty-folks.h"
#include "chatty-window.h"
#include "chatty-contact-row.h"
#include "chatty-utils.h"
#include "chatty-icons.h"
#include "chatty-window.h"
#include <libebook-contacts/libebook-contacts.h>



typedef struct {
  FolksIndividual  *individual;
  ChattyContactRow *row;
  PurpleAccount    *purple_account;
  const char       *purple_user_name;
  int               mode;
  int               size;
} AvatarData;


static void
cb_pixbuf_from_stream_ready (GObject      *source_object,
                             GAsyncResult *result,
                             gpointer      avatar_data)
{
  GdkPixbuf     *pixbuf;
  AvatarData    *data = avatar_data;
  PurpleContact *contact;
  PurpleBuddy   *buddy;
  gchar         *buffer;
  size_t         size;
  GError        *error = NULL;

  pixbuf = gdk_pixbuf_new_from_stream_finish (result, &error);

  if (error != NULL) {
    g_debug ("Could not get pixbuf from stream: %s", error->message);
    g_slice_free (AvatarData, data);
    g_error_free (error);

    return;
  }

  switch (data->mode) {
    case CHATTY_FOLKS_SET_CONTACT_ROW_ICON:
      g_return_if_fail (data->row != NULL);

      g_object_set (data->row,
                    "avatar", chatty_icon_shape_pixbuf_circular (pixbuf),
                    NULL);
      break;

    case CHATTY_FOLKS_SET_PURPLE_BUDDY_ICON:
      gdk_pixbuf_save_to_buffer (pixbuf, &buffer, &size, "png", &error, NULL);

      if (error != NULL) {
        g_debug ("Could not save pixbuf to buffer: %s", error->message);
        g_slice_free (AvatarData, data);
        g_error_free (error);

        return;
      }

      buddy = purple_find_buddy (data->purple_account, data->purple_user_name);
      contact = purple_buddy_get_contact (buddy);

      purple_buddy_icons_node_set_custom_icon ((PurpleBlistNode*)contact, 
                                               (guchar*)buffer, 
                                               size);
      break;

    default:
      g_warning ("Chatty folks icon mode not set");
  }
  
  g_object_unref (pixbuf);

  g_slice_free (AvatarData, data);
}


static void
cb_icon_load_async_ready (GObject      *source_object,
                          GAsyncResult *result,
                          gpointer      avatar_data)
{
  GLoadableIcon *icon = G_LOADABLE_ICON (source_object);
  AvatarData    *data = avatar_data;
  GInputStream  *stream;
  GError        *error = NULL;

  stream = g_loadable_icon_load_finish (icon, result, NULL, &error);

  if (error != NULL) {
    g_debug ("Could not load icon: %s", error->message);
    g_slice_free (AvatarData, data);
    g_error_free (error);

    return;
  }

  gdk_pixbuf_new_from_stream_at_scale_async (stream,
                                             data->size, 
                                             data->size, 
                                             TRUE,
                                             NULL,
                                             cb_pixbuf_from_stream_ready, 
                                             data);
                                            
  g_object_unref (stream);
}


/**
 * chatty_folks_load_avatar:
 * 
 * @individual: a folks individual
 * @row:        a ChattyContactsRow
 * @account:    a PurpleAccount
 * @user_name:  a purple buddy name
 * @mode:       sets the icon target
 * @size:       the icon size
 * 
 * Loads a folks avatar and stores it either
 * in the given contacts-row or in blist.xml
 * depending on mode.
 * 
 * If no folks avatar is available, a green icon 
 * with the initial of the contact name will be 
 * created.
 * 
 */
void
chatty_folks_load_avatar (FolksIndividual  *individual,
                          ChattyContactRow *row,
                          PurpleAccount    *account,
                          const char       *user_name,
                          int               mode,
                          int               size)
{
  AvatarData    *data;
  GLoadableIcon *avatar;
  GdkPixbuf     *pixbuf;
  const char    *name;

  g_return_if_fail (FOLKS_IS_INDIVIDUAL (individual));

  avatar = folks_avatar_details_get_avatar (FOLKS_AVATAR_DETAILS(individual));

  if (avatar == NULL && mode == CHATTY_FOLKS_SET_CONTACT_ROW_ICON) {
    g_return_if_fail (row);

    name = folks_individual_get_display_name (individual);

    pixbuf = chatty_icon_get_buddy_icon (NULL,
                                         name,
                                         CHATTY_ICON_SIZE_MEDIUM,
                                         CHATTY_COLOR_GREEN,
                                         FALSE);
    
    g_object_set (row,
                  "avatar", chatty_icon_shape_pixbuf_circular (pixbuf),
                  NULL);

    g_object_unref (pixbuf);

    return;
  } else if (G_IS_LOADABLE_ICON(avatar)) {
    data = g_slice_new0 (AvatarData);
    data->row = row;
    data->mode = mode;
    data->size = size;
    data->purple_account = account;
    data->purple_user_name = user_name;

    g_loadable_icon_load_async (avatar, size, NULL, cb_icon_load_async_ready, data);
  }
}


/**
 * chatty_folks_set_purple_buddy_avatar:
 * 
 * @folks_id:  a const char with the ID of an individual
 * @account:   a purple account
 * @user_name: a purple user name
 * 
 * Set a buddy icon from folks avatar data
 *
 */
void
chatty_folks_set_purple_buddy_data (ChattyContact *chatty_contact,
                                    PurpleAccount *account,
                                    const char    *user_name)
{ 
  FolksIndividual *individual;

  individual = chatty_contact_get_individual (chatty_contact);

  if (individual != NULL) {
    PurpleBuddy *buddy = purple_find_buddy (account, user_name);
    PurpleContact *contact = purple_buddy_get_contact (buddy);
    purple_contact_set_alias (contact, folks_individual_get_display_name (individual));

    chatty_folks_load_avatar (individual, 
                              NULL, 
                              account, 
                              user_name, 
                              CHATTY_FOLKS_SET_PURPLE_BUDDY_ICON, 
                              CHATTY_ICON_SIZE_LARGE);
  }
}
