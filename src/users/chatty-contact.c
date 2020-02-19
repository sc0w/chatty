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

#include <gee-0.8/gee.h>
#include <folks/folks.h>
#include <libebook-contacts/libebook-contacts.h>

#include "chatty-contact.h"

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
  FolksPhoneFieldDetails *phone_number;
};

G_DEFINE_TYPE (ChattyContact, chatty_contact, CHATTY_TYPE_ITEM)


/* Always assume it’s a phone number, we create only such contacts */
static ChattyProtocol
chatty_contact_get_protocols (ChattyItem *item)
{
  return CHATTY_PROTOCOL_SMS;
}


static gboolean
chatty_contact_matches (ChattyItem     *item,
                        const char     *needle,
                        ChattyProtocol  protocols,
                        gboolean        match_name)
{
  ChattyContact *self = (ChattyContact *)item;
  char *number;
  EPhoneNumberMatch number_match;

  g_assert (CHATTY_IS_CONTACT (self));

  number = folks_phone_field_details_get_normalised (self->phone_number);
  number_match = e_phone_number_compare_strings (number, needle, NULL);

  if (number_match == E_PHONE_NUMBER_MATCH_EXACT ||
      number_match == E_PHONE_NUMBER_MATCH_NATIONAL)
    return TRUE;

  return FALSE;
}


static const char *
chatty_contact_get_name (ChattyItem *item)
{
  ChattyContact *self = (ChattyContact *)item;

  g_assert (CHATTY_IS_CONTACT (self));

  return folks_individual_get_display_name (self->individual);
}


static void
chatty_contact_finalize (GObject *object)
{
  ChattyContact *self = (ChattyContact *)object;

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
}


static void
chatty_contact_init (ChattyContact *self)
{
}


ChattyContact *
chatty_contact_new (FolksIndividual        *individual,
                    FolksPhoneFieldDetails *detail)
{
  ChattyContact *self;

  g_return_val_if_fail (FOLKS_IS_INDIVIDUAL (individual), NULL);

  self = g_object_new (CHATTY_TYPE_CONTACT, NULL);
  self->individual = individual;
  self->phone_number = detail;

  return self;
}


const char *
chatty_contact_get_phone_number (ChattyContact *self)
{
  g_return_val_if_fail (CHATTY_IS_CONTACT (self), NULL);

  return folks_phone_field_details_get_normalised (self->phone_number);
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


FolksPhoneFieldDetails *
chatty_contact_get_detail (ChattyContact *self)
{
  g_return_val_if_fail (CHATTY_IS_CONTACT (self), NULL);

  return self->phone_number;
}
