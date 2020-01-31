/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-buddy.h
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
#include <purple.h>

#include "chatty-enums.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_PP_BUDDY (chatty_pp_buddy_get_type ())

G_DECLARE_FINAL_TYPE (ChattyPpBuddy, chatty_pp_buddy, CHATTY, PP_BUDDY, ChattyUser)

PurpleAccount   *chatty_pp_buddy_get_account   (ChattyPpBuddy *self);
PurpleBuddy     *chatty_pp_buddy_get_buddy      (ChattyPpBuddy *self);

G_END_DECLS
