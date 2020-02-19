/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-folks.c
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-folks"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <gee-0.8/gee.h>
#include <folks/folks.h>

#include "users/chatty-contact.h"
#include "chatty-contact-provider.h"

/**
 * SECTION: chatty-folks
 * @title: ChattyFolks
 * @short_description: A Contact List populated with libfolks
 * @include: "chatty-folks.h"
 */

struct _ChattyFolks
{
  GObject     parent_instance;

  GListStore *contact_list;

  FolksIndividualAggregator *folks_aggregator;
};

G_DEFINE_TYPE (ChattyFolks, chatty_folks, G_TYPE_OBJECT)


static ChattyContact *
chatty_contact_provider_matches (ChattyFolks    *self,
                                 const char     *needle,
                                 ChattyProtocol  protocols,
                                 gboolean        match_name)
{
  GListModel *model;
  guint n_items;
  gboolean match;

  model = G_LIST_MODEL (self->contact_list);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(ChattyItem) item = NULL;

      item = g_list_model_get_item (model, i);
      match = chatty_item_matches (item, needle, protocols, match_name);

      if (match)
        return CHATTY_CONTACT (item);
    }

  return NULL;
}


static void
folks_aggregator_is_quiescent_cb (ChattyFolks *self)
{
  GeeMap *individuals;
  GeeMapIterator *iter;
  FolksIndividual *individual;
  g_autoptr(GPtrArray) array = NULL;

  g_assert (CHATTY_IS_FOLKS (self));

  individuals = folks_individual_aggregator_get_individuals (self->folks_aggregator);
  iter = gee_map_map_iterator (individuals);
  array = g_ptr_array_new_full (100, g_object_unref);

  while (gee_map_iterator_next (iter))
    {
      GeeIterator *phone_iter;
      GeeSet *phone_numbers;

      individual = gee_map_iterator_get_value (iter);
      g_object_get (individual, "phone-numbers", &phone_numbers, NULL);
      phone_iter = gee_iterable_iterator (GEE_ITERABLE (phone_numbers));

      while (gee_iterator_next (phone_iter))
        {
          ChattyContact *contact;

          contact = chatty_contact_new (individual,
                                        gee_iterator_get (phone_iter));
          g_ptr_array_add (array, contact);
        }
      g_object_unref (phone_iter);
      g_object_unref (phone_numbers);
    }

  g_list_store_splice (self->contact_list, 0, 0, array->pdata, array->len);
  g_object_unref (iter);
}


static void
chatty_folks_finalize (GObject *object)
{
  ChattyFolks *self = (ChattyFolks *)object;

  g_clear_object (&self->contact_list);

  G_OBJECT_CLASS (chatty_folks_parent_class)->finalize (object);
}


static void
chatty_folks_class_init (ChattyFolksClass *klass)
{
  GObjectClass *object_class  = G_OBJECT_CLASS (klass);

  object_class->finalize = chatty_folks_finalize;
}


static void
chatty_folks_init (ChattyFolks *self)
{
  self->contact_list = g_list_store_new (CHATTY_TYPE_CONTACT);
  self->folks_aggregator = folks_individual_aggregator_dup ();

  g_signal_connect_object (self->folks_aggregator,
                           "notify::is-quiescent",
                           G_CALLBACK (folks_aggregator_is_quiescent_cb),
                           self, G_CONNECT_SWAPPED);

  folks_individual_aggregator_prepare (self->folks_aggregator, NULL, NULL);
}


ChattyFolks *
chatty_folks_new (void)
{
  return g_object_new (CHATTY_TYPE_FOLKS, NULL);
}


GListModel *
chatty_folks_get_model (ChattyFolks *self)
{
  g_return_val_if_fail (CHATTY_IS_FOLKS (self), NULL);

  return G_LIST_MODEL (self->contact_list);
}


ChattyContact *
chatty_folks_find_by_name (ChattyFolks *self,
                           const char  *name)
{
  g_return_val_if_fail (CHATTY_IS_FOLKS (self), NULL);

  return chatty_contact_provider_matches (self, name, CHATTY_PROTOCOL_ANY, TRUE);
}


ChattyContact *
chatty_folks_find_by_number (ChattyFolks *self,
                             const char  *phone_number)
{
  g_return_val_if_fail (CHATTY_IS_FOLKS (self), NULL);

  return chatty_contact_provider_matches (self, phone_number, CHATTY_PROTOCOL_ANY, FALSE);
}
