/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-user.h
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>
#include <gdk/gdk.h>

#include "chatty-enums.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_USER (chatty_user_get_type ())

G_DECLARE_DERIVABLE_TYPE (ChattyUser, chatty_user, CHATTY, USER, GObject)

struct _ChattyUserClass
{
  GObjectClass parent_class;

  const char      *(*get_name)            (ChattyUser           *self);
  void             (*set_name)            (ChattyUser           *self,
                                           const char           *name);
};

const char      *chatty_user_get_name            (ChattyUser           *self);
void             chatty_user_set_name            (ChattyUser           *self,
                                                  const char           *name);
G_END_DECLS
