/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-contact.c
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-contact"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>
#include <gee-0.8/gee.h>
#include <folks/folks.h>
#include <gtk/gtk.h>
#include <libebook-contacts/libebook-contacts.h>

#include "chatty-icons.h"
#include "chatty-contact.h"
#include "chatty-contact-private.h"

#define ICON_SIZE 96

/**
 * SECTION: chatty-contact
 * @title: ChattyContact
 * @short_description: An abstraction over #FolksIndividual
 * @include: "chatty-contact.h"
 */

struct _ChattyContact
{
  ChattyItem       parent_instance;

  EContact        *e_contact;
  EVCardAttribute *attribute;
  ChattyProtocol   protocol;

  GdkPixbuf *avatar;
};

G_DEFINE_TYPE (ChattyContact, chatty_contact, CHATTY_TYPE_ITEM)


/* Always assume it’s a phone number, we create only such contacts */
static ChattyProtocol
chatty_contact_get_protocols (ChattyItem *item)
{
  ChattyContact *self = (ChattyContact *)item;

  g_assert (CHATTY_IS_CONTACT (self));

  return self->protocol;
}


static gboolean
chatty_contact_matches (ChattyItem     *item,
                        const char     *needle,
                        ChattyProtocol  protocols,
                        gboolean        match_name)
{
  ChattyContact *self = (ChattyContact *)item;
  const char *value;
  ChattyProtocol protocol;

  g_assert (CHATTY_IS_CONTACT (self));

  value = chatty_contact_get_value (self);
  protocol = chatty_item_get_protocols (item);

  if (protocol == CHATTY_PROTOCOL_SMS &&
      protocols & CHATTY_PROTOCOL_SMS) {
    EPhoneNumberMatch match;

    match = e_phone_number_compare_strings (value, needle, NULL);

    if (match == E_PHONE_NUMBER_MATCH_EXACT ||
        match == E_PHONE_NUMBER_MATCH_NATIONAL)
      return TRUE;

    if (g_str_equal (value, needle))
      return TRUE;
  }

  return FALSE;
}


static const char *
chatty_contact_get_name (ChattyItem *item)
{
  ChattyContact *self = (ChattyContact *)item;
  const char *value;

  g_assert (CHATTY_IS_CONTACT (self));

  value = e_contact_get_const (self->e_contact, E_CONTACT_FULL_NAME);

  if (!value)
    value = "";

  return value;
}


static GdkPixbuf *
chatty_contact_get_avatar (ChattyItem *item)
{
  ChattyContact *self = (ChattyContact *)item;
  g_autoptr(GError) error = NULL;
  EContactPhoto *photo;

  g_assert (CHATTY_IS_CONTACT (self));

  if (self->avatar)
    return self->avatar;

  photo = e_contact_get (self->e_contact, E_CONTACT_PHOTO);

  if (!photo)
    return NULL;

  if (photo->type == E_CONTACT_PHOTO_TYPE_URI) {
    g_autoptr(GFileInputStream) stream = NULL;
    g_autoptr(GFile) file = NULL;

    file = g_file_new_for_uri (e_contact_photo_get_uri (photo));
    stream = g_file_read (file, NULL, NULL);

    if (stream)
      self->avatar = gdk_pixbuf_new_from_stream (G_INPUT_STREAM (stream), NULL, NULL);
  } else {
    const guchar *data;
    gsize len;

    data = e_contact_photo_get_inlined (photo, &len);

    if (data)
      self->avatar = chatty_icon_pixbuf_from_data (data, len);
  }

  return self->avatar;
}

static void
chatty_contact_get_avatar_async (ChattyItem          *item,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  ChattyContact *self = (ChattyContact *)item;
  g_autoptr(GTask) task = NULL;

  g_assert (CHATTY_IS_CONTACT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  self->avatar = chatty_contact_get_avatar (item);
  g_task_return_pointer (task, self->avatar, NULL);
}

static void
chatty_contact_finalize (GObject *object)
{
  ChattyContact *self = (ChattyContact *)object;

  g_object_unref (self->avatar);

  G_OBJECT_CLASS (chatty_contact_parent_class)->finalize (object);
}


static void
chatty_contact_class_init (ChattyContactClass *klass)
{
  GObjectClass *object_class  = G_OBJECT_CLASS (klass);
  ChattyItemClass *item_class = CHATTY_ITEM_CLASS (klass);

  object_class->finalize = chatty_contact_finalize;

  item_class->get_protocols = chatty_contact_get_protocols;
  item_class->matches  = chatty_contact_matches;
  item_class->get_name = chatty_contact_get_name;
  item_class->get_avatar = chatty_contact_get_avatar;
  item_class->get_avatar_async  = chatty_contact_get_avatar_async;
}


static void
chatty_contact_init (ChattyContact *self)
{
}


/**
 * chatty_contact_new:
 * @contact: A #EContact
 * @attr: (transfer full): A #EvCardAttribute
 * @protocol: A #ChattyProtocol for the attribute @attr
 *
 * Create a new contact which represents the @attr of
 * the @contact.
 *
 * Currently, only %CHATTY_PROTOCOL_CALL and %CHATTY_PROTOCOL_SMS
 * is supported as @protocol.
 *
 * Returns: (transfer full): A #ChattyContact
 */
ChattyContact *
chatty_contact_new (EContact        *contact,
                    EVCardAttribute *attr,
                    ChattyProtocol   protocol)
{
  ChattyContact *self;

  self = g_object_new (CHATTY_TYPE_CONTACT, NULL);
  self->e_contact = g_object_ref (contact);
  self->attribute = attr;
  self->protocol  = protocol;

  return self;
}

/**
 * chatty_contact_get_value:
 * @self: A #ChattyContact
 *
 * Get the value stored in @self. It can be a phone
 * number, an XMPP ID, etc.
 * Also see chatty_contact_get_value_type().
 *
 * Returns: (transfer none): The value of @self.
 * Or an empty string if no value.
 */
const char *
chatty_contact_get_value (ChattyContact *self)
{
  const char *value;

  g_return_val_if_fail (CHATTY_IS_CONTACT (self), NULL);

  value = e_vcard_attribute_get_value (self->attribute);

  if (!value)
    value = "";

  return value;
}

/**
 * chatty_contact_get_value:
 * @self: A #ChattyContact
 *
 * Get the type of value stored in @self.
 * Eg: “Mobile”, “Work”, etc. translated to
 * the current locale.
 *
 * Returns: (transfer none) (nullable): The value type of @self.
 */
const char *
chatty_contact_get_value_type (ChattyContact *self)
{
  g_return_val_if_fail (CHATTY_IS_CONTACT (self), NULL);

  if (e_vcard_attribute_has_type (self->attribute, "cell"))
    return _("Mobile");
  if (e_vcard_attribute_has_type (self->attribute, "work"))
    return _("Work");
  if (e_vcard_attribute_has_type (self->attribute, "other"))
    return _("Other");

  return NULL;
}


/**
 * chatty_contact_get_uid:
 * @self: A #ChattyContact
 *
 * A unique ID reperesenting the contact.  This
 * ID won’t change unless the contact is modified.
 *
 * Returns: (transfer none): A unique ID of @self.
 */
const char *
chatty_contact_get_uid (ChattyContact *self)
{
  g_return_val_if_fail (CHATTY_IS_CONTACT (self), "");

  return e_contact_get_const (self->e_contact, E_CONTACT_UID);
}


/**
 * chatty_contact_get_individual:
 * @self: A #ChattyContact
 *
 * Get the #FolksIndividual used to create @self.
 * Use this only for debug purposes.
 *
 * Returns: (transfer none): A #FolksIndividual
 */
FolksIndividual *
chatty_contact_get_individual (ChattyContact *self)
{
  g_return_val_if_fail (CHATTY_IS_CONTACT (self), NULL);

  return NULL;
}


/**
 * chatty_contact_clear_cache:
 * @self: #ChattyContact
 *
 * Reset the values cached in @self.
 * This API is only to be used by contact-provider.
 */
void
chatty_contact_clear_cache (ChattyContact *self)
{
  g_return_if_fail (CHATTY_IS_CONTACT (self));

  g_clear_object (&self->avatar);
}
