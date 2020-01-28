/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-avatar.h
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

#include "users/chatty-user.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_AVATAR (chatty_avatar_get_type ())

G_DECLARE_FINAL_TYPE (ChattyAvatar, chatty_avatar, CHATTY, AVATAR, GtkImage)

GtkWidget *chatty_avatar_new      (ChattyUser   *user);
void       chatty_avatar_set_user (ChattyAvatar *self,
                                   ChattyUser   *user);

G_END_DECLS
