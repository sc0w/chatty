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
#include <libebook-contacts/libebook-contacts.h>

#include "chatty-contact.h"

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

  FolksIndividual *individual;
  FolksAbstractFieldDetails *detail;

  char *value;
  GdkPixbuf *avatar;
};

G_DEFINE_TYPE (ChattyContact, chatty_contact, CHATTY_TYPE_ITEM)

static char *
chatty_contact_check_phonenumber (const char *phone_number)
{
  EPhoneNumber      *number;
  char              *result;
  g_autoptr(GError)  err = NULL;

  number = e_phone_number_from_string (phone_number, NULL, &err);

  if (!number || !e_phone_number_is_supported ()) {
    g_debug ("%s %s: %s\n", __func__, phone_number, err->message);

    result = NULL;
  } else {
    if (g_strrstr (phone_number, "+")) {
      result = e_phone_number_to_string (number, E_PHONE_NUMBER_FORMAT_E164);
    } else {
      result = e_phone_number_get_national_number (number);
    }
  }

  e_phone_number_free (number);

  return result;
}


static void
load_avatar_finish_cb (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  ChattyContact *self = user_data;
  GLoadableIcon *icon = G_LOADABLE_ICON (object);
  GInputStream  *stream;
  g_autoptr(GError) error = NULL;

  g_assert (CHATTY_IS_CONTACT (self));

  stream = g_loadable_icon_load_finish (icon, result, NULL, &error);

  if (error) {
    g_debug ("Could not load icon: %s", error->message);

    return;
  }

  self->avatar = gdk_pixbuf_new_from_stream_at_scale (stream,
                                                      ICON_SIZE,
                                                      ICON_SIZE,
                                                      TRUE,
                                                      NULL,
                                                      &error);
  if (error)
    g_debug ("Could not load icon: %s", error->message);
  else
    g_signal_emit_by_name (self, "avatar-changed");
}

/* Always assume it’s a phone number, we create only such contacts */
static ChattyProtocol
chatty_contact_get_protocols (ChattyItem *item)
{
  ChattyContact *self = (ChattyContact *)item;

  g_assert (CHATTY_IS_CONTACT (self));

  if (FOLKS_IS_PHONE_FIELD_DETAILS (self->detail))
    return CHATTY_PROTOCOL_SMS;

  return CHATTY_PROTOCOL_NONE;
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

  g_assert (CHATTY_IS_CONTACT (self));

  return folks_individual_get_display_name (self->individual);
}


static GdkPixbuf *
chatty_contact_get_avatar (ChattyItem *item)
{
  ChattyContact *self = (ChattyContact *)item;
  GLoadableIcon *avatar;

  g_assert (CHATTY_IS_CONTACT (self));

  if (self->avatar)
    return self->avatar;

  avatar = folks_avatar_details_get_avatar (FOLKS_AVATAR_DETAILS (self->individual));

  if (avatar)
    g_loadable_icon_load_async (avatar, ICON_SIZE, NULL, load_avatar_finish_cb, self);

  return NULL;
}

static void
chatty_contact_finalize (GObject *object)
{
  ChattyContact *self = (ChattyContact *)object;

  g_object_unref (self->avatar);
  g_free (self->value);

  g_object_unref (self->detail);
  g_object_unref (self->individual);

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
}


static void
chatty_contact_init (ChattyContact *self)
{
}


ChattyContact *
chatty_contact_new (FolksIndividual           *individual,
                    FolksAbstractFieldDetails *detail)
{
  ChattyContact *self;

  g_return_val_if_fail (FOLKS_IS_INDIVIDUAL (individual), NULL);

  self = g_object_new (CHATTY_TYPE_CONTACT, NULL);
  self->individual = g_object_ref (individual);
  self->detail = detail;

  return self;
}


const char *
chatty_contact_get_value (ChattyContact *self)
{
  ChattyProtocol protocol;

  g_return_val_if_fail (CHATTY_IS_CONTACT (self), NULL);

  protocol = chatty_item_get_protocols (CHATTY_ITEM (self));

  if (!self->value && protocol == CHATTY_PROTOCOL_SMS) {
    FolksPhoneFieldDetails *phone;
    g_autofree char *number = NULL;

    phone  = FOLKS_PHONE_FIELD_DETAILS (self->detail);
    number = folks_phone_field_details_get_normalised (phone);

    self->value = chatty_contact_check_phonenumber (number);
    g_assert (self->value);
  }

  if (self->value)
    return self->value;

  return "";
}

const char *
chatty_contact_get_value_type (ChattyContact *self)
{
  GeeCollection *types;
  GeeIterator *iter;

  g_return_val_if_fail (CHATTY_IS_CONTACT (self), NULL);

  types = folks_abstract_field_details_get_parameter_values (self->detail, "type");

  if (types == NULL)
    return NULL;

  iter = gee_iterable_iterator (GEE_ITERABLE (types));

  while (gee_iterator_next (iter)) {
    g_autofree char *type = gee_iterator_get (iter);

    if (g_strcmp0 (type, "cell") == 0)
      return _("Mobile");
    if (g_strcmp0 (type, "work") == 0)
      return _("Work");
    if (g_strcmp0 (type, "home") == 0)
      return _("Home");
    else if (g_strcmp0 (type, "other") == 0)
      return _("Other");
  }

  g_object_unref (iter);

  return NULL;
}


const char *
chatty_contact_get_uid (ChattyContact *self)
{
  g_return_val_if_fail (CHATTY_IS_CONTACT (self), "");

  return folks_individual_get_id (self->individual);
}


FolksIndividual *
chatty_contact_get_individual (ChattyContact *self)
{
  g_return_val_if_fail (CHATTY_IS_CONTACT (self), NULL);

  return self->individual;
}
