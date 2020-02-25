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

static void
contact_pixbuf_load_finish_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  ChattyContact *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (CHATTY_IS_CONTACT (self));

  self->avatar = gdk_pixbuf_new_from_stream_finish (result, &error);

  if (error) {
    g_task_return_error (task, error);

    return;
  }

  g_signal_emit_by_name (self, "avatar-changed");
  g_task_return_pointer (task, self->avatar, NULL);
}

static void
load_avatar_async_finish_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  GLoadableIcon *icon = G_LOADABLE_ICON (object);
  GCancellable *cancellable;
  GInputStream  *stream;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));

  stream = g_loadable_icon_load_finish (icon, result, NULL, &error);
  cancellable = g_task_get_cancellable (task);

  if (error) {
    g_task_return_error (task, error);

    return;
  }

  gdk_pixbuf_new_from_stream_at_scale_async (stream,
                                             ICON_SIZE,
                                             ICON_SIZE,
                                             TRUE,
                                             cancellable,
                                             contact_pixbuf_load_finish_cb,
                                             g_steal_pointer (&task));
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
chatty_contact_get_avatar_async (ChattyItem          *item,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  ChattyContact *self = (ChattyContact *)item;
  g_autoptr(GTask) task = NULL;
  GLoadableIcon *avatar;

  g_assert (CHATTY_IS_CONTACT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  if (self->avatar) {
    g_task_return_pointer (task, self->avatar, NULL);

    return;
  }

  avatar = folks_avatar_details_get_avatar (FOLKS_AVATAR_DETAILS (self->individual));

  if (!avatar)
    g_task_return_pointer (task, NULL, NULL);
  else
    g_loadable_icon_load_async (avatar, ICON_SIZE, cancellable,
                                load_avatar_async_finish_cb,
                                g_steal_pointer (&task));
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
  item_class->get_avatar_async  = chatty_contact_get_avatar_async;
}


static void
chatty_contact_init (ChattyContact *self)
{
}


/**
 * chatty_contact_new:
 * @individual: A #FolksIndividual
 * @detail: (transfer full): A #FolksAbstractFieldDetails
 *
 * Create a new contact which represents the @detail of
 * the @individual.
 *
 * Currently, only #FolksPhoneFieldDetails is supported
 * as @detail.
 *
 * Returns: (transfer full): A #ChattyContact
 */
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

  return folks_individual_get_id (self->individual);
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

  return self->individual;
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
