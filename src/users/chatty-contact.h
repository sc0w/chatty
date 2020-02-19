/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-contact.h
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>
#include <folks/folks.h>

#include "users/chatty-item.h"
#include "chatty-enums.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_CONTACT (chatty_contact_get_type ())

G_DECLARE_FINAL_TYPE (ChattyContact, chatty_contact, CHATTY, CONTACT, ChattyItem)

ChattyContact     *chatty_contact_new                   (FolksIndividual   *individual,
                                                         FolksPhoneFieldDetails *detail);
const char        *chatty_contact_get_phone_number      (ChattyContact     *self);
const char        *chatty_contact_get_uid               (ChattyContact     *self);
FolksIndividual   *chatty_contact_get_individual        (ChattyContact     *self);
FolksPhoneFieldDetails *chatty_contact_get_detail       (ChattyContact     *self);

G_END_DECLS
