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


static void
folks_find_contact_index (ChattyFolks     *self,
                          FolksIndividual *individual,
                          guint           *position,
                          guint           *count)
{
  const char *old_id, *id;
  guint n_items, i;

  g_assert (CHATTY_IS_FOLKS (self));
  g_assert (FOLKS_IS_INDIVIDUAL (individual));
  g_assert (position && count);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->contact_list));
  *position = *count = 0;

  for (i = 0; i < n_items; i++) {
    g_autoptr(ChattyContact) contact = NULL;

    contact = g_list_model_get_item (G_LIST_MODEL (self->contact_list), i);
    old_id = chatty_contact_get_uid (contact);
    id = folks_individual_get_id (individual);

    /* The list is sorted. Find the first and last matching item indices. */
    if (!*count && old_id == id)
      *position = i, *count = 1;
    else if (old_id == id)
      ++*count;
    else if (*position)
      break;
  }
}


static void
folk_item_changed_cb (ChattyFolks     *self,
                      GParamSpec      *pspec,
                      FolksIndividual *individual)
{
  guint position, count;

  g_assert (CHATTY_IS_FOLKS (self));
  g_assert (FOLKS_IS_INDIVIDUAL (individual));

  folks_find_contact_index (self, individual, &position, &count);

  if (count)
    g_list_model_items_changed (G_LIST_MODEL (self->contact_list),
                                position, count, count);
}


static void folks_remove_contact (ChattyFolks     *self,
                                  FolksIndividual *individual);
static void folks_load_details   (ChattyFolks     *self,
                                  FolksIndividual *individual,
                                  GPtrArray       *store);
static void
folk_detail_changed_cb (ChattyFolks     *self,
                        GParamSpec      *pspec,
                        FolksIndividual *individual)
{
  g_assert (CHATTY_IS_FOLKS (self));
  g_assert (FOLKS_IS_INDIVIDUAL (individual));

  folks_remove_contact (self, individual);
  folks_load_details (self, individual, NULL);
}


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
folks_connect_signals (ChattyFolks     *self,
                       FolksIndividual *individual)
{
  g_signal_connect_object (G_OBJECT (individual),
                           "notify::avatar",
                           G_CALLBACK (folk_item_changed_cb),
                           self, G_CONNECT_SWAPPED);

  g_signal_connect_object (G_OBJECT (individual),
                           "notify::display-name",
                           G_CALLBACK (folk_item_changed_cb),
                           self, G_CONNECT_SWAPPED);

  g_signal_connect_object (G_OBJECT (individual),
                           "notify::phone-numbers",
                           G_CALLBACK (folk_detail_changed_cb),
                           self, G_CONNECT_SWAPPED);
}

static void
folks_load_details (ChattyFolks     *self,
                    FolksIndividual *individual,
                    GPtrArray       *store)
{
  GeeIterator *phone_iter;
  GeeSet *phone_numbers;
  g_autoptr(GPtrArray) array = NULL;

  if (!store) {
    array = g_ptr_array_new_full (1, g_object_unref);
    store = array;
  }

  g_object_get (individual, "phone-numbers", &phone_numbers, NULL);
  phone_iter = gee_iterable_iterator (GEE_ITERABLE (phone_numbers));
  if (gee_collection_get_is_empty (GEE_COLLECTION (phone_numbers)))
    return;

  while (gee_iterator_next (phone_iter)) {
    ChattyContact *contact;

    contact = chatty_contact_new (individual,
                                  gee_iterator_get (phone_iter));
    g_ptr_array_add (store, contact);
  }

  if (array)
    g_list_store_splice (self->contact_list, 0, 0, array->pdata, array->len);

  g_object_unref (phone_iter);
  g_object_unref (phone_numbers);
}


static void
folks_remove_contact (ChattyFolks     *self,
                      FolksIndividual *individual)
{
  guint position, count;

  g_assert (CHATTY_IS_FOLKS (self));
  g_assert (FOLKS_IS_INDIVIDUAL (individual));

  folks_find_contact_index (self, individual, &position, &count);

  if (count)
    g_list_store_splice (self->contact_list, position, count, NULL, 0);
}


static void
folks_individual_changed_cb (ChattyFolks *self,
                             GeeMultiMap *changes)
{
  GeeMapIterator *iter;
  g_autoptr(GPtrArray) added = NULL;
  g_autoptr(GPtrArray) removed = NULL;

  g_assert (CHATTY_IS_FOLKS (self));

  iter = gee_multi_map_map_iterator (changes);
  added = g_ptr_array_new_full (1, g_object_unref);
  removed = g_ptr_array_new_full (1, g_object_unref);

  while (gee_map_iterator_next (iter)) {
    FolksIndividual *key, *value;

    key = gee_map_iterator_get_key (iter);
    value = gee_map_iterator_get_value (iter);

    if (key && !g_ptr_array_find (removed, key, NULL))
      g_ptr_array_add (removed, key);

    if (value && !g_ptr_array_find (added, value, NULL))
      g_ptr_array_add (added, value);
  }

  for (guint i = 0; i < removed->len; i++)
    folks_remove_contact (self, removed->pdata[i]);

  for (guint i = 0; i < added->len; i++) {
    folks_load_details (self, added->pdata[i], NULL);
    folks_connect_signals (self, added->pdata[i]);
  }
}


static void
folks_aggregator_is_quiescent_cb (ChattyFolks *self)
{
  GeeMap *individuals;
  GeeMapIterator *iter;
  g_autoptr(GPtrArray) array = NULL;

  g_assert (CHATTY_IS_FOLKS (self));

  individuals = folks_individual_aggregator_get_individuals (self->folks_aggregator);
  iter = gee_map_map_iterator (individuals);
  array = g_ptr_array_new_full (100, g_object_unref);

  while (gee_map_iterator_next (iter)) {
    folks_load_details (self, gee_map_iterator_get_value (iter), array);
    folks_connect_signals (self, gee_map_iterator_get_value (iter));
  }

  g_list_store_splice (self->contact_list, 0, 0, array->pdata, array->len);
  g_object_unref (iter);

  g_signal_connect_object (self->folks_aggregator,
                           "individuals-changed-detailed",
                           G_CALLBACK (folks_individual_changed_cb),
                           self, G_CONNECT_SWAPPED);

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
