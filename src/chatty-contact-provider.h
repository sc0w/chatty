/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-folks.h
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
#include <gdk/gdk.h>

#include "users/chatty-contact.h"


G_BEGIN_DECLS

#define CHATTY_TYPE_FOLKS (chatty_folks_get_type ())

G_DECLARE_FINAL_TYPE (ChattyFolks, chatty_folks, CHATTY, FOLKS, GObject)

ChattyFolks   *chatty_folks_new            (void);
gboolean       chatty_folks_is_ready       (ChattyFolks *self);
GListModel    *chatty_folks_get_model      (ChattyFolks *self);
ChattyContact *chatty_folks_find_by_name   (ChattyFolks *self,
                                            const char  *name);
ChattyContact *chatty_folks_find_by_number (ChattyFolks *self,
                                            const char  *phone_number);

G_END_DECLS
