/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-pp-account.h
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

#define CHATTY_TYPE_PP_ACCOUNT (chatty_pp_account_get_type ())

G_DECLARE_FINAL_TYPE (ChattyPpAccount, chatty_pp_account, CHATTY, PP_ACCOUNT, GObject)

ChattyPpAccount *chatty_pp_account_new                (const char      *username,
                                                       const char      *protocol_id);
ChattyPpAccount *chatty_pp_account_new_purple         (PurpleAccount   *account);
void             chatty_pp_account_save               (ChattyPpAccount *self);
PurpleAccount   *chatty_pp_account_get_account        (ChattyPpAccount *self);
PurpleStatus    *chatty_pp_account_get_active_status  (ChattyPpAccount *self);
ChattyStatus     chatty_pp_account_get_status         (ChattyPpAccount *self);
gboolean         chatty_pp_account_is_sms             (ChattyPpAccount *self);

const char      *chatty_pp_account_get_protocol_id    (ChattyPpAccount *self);
const char      *chatty_pp_account_get_protocol_name  (ChattyPpAccount *self);
void             chatty_pp_account_set_enabled        (ChattyPpAccount *self,
                                                       gboolean         enable);
gboolean         chatty_pp_account_get_enabled        (ChattyPpAccount *self);

void             chatty_pp_account_set_username       (ChattyPpAccount *self,
                                                       const char      *username);
const char      *chatty_pp_account_get_username       (ChattyPpAccount *self);

void             chatty_pp_account_set_password       (ChattyPpAccount *self,
                                                       const char      *password);
const char      *chatty_pp_account_get_password       (ChattyPpAccount *self);
void             chatty_pp_account_set_remember_password (ChattyPpAccount *self,
                                                          gboolean         remember);
gboolean         chatty_pp_account_get_remember_password (ChattyPpAccount *self);

void             chatty_pp_account_add_buddy          (ChattyPpAccount *self,
                                                       PurpleBuddy     *buddy);
void             chatty_pp_account_add_buddy_and_invite (ChattyPpAccount *self,
                                                         PurpleBuddy     *buddy);

/* void             chatty_pp_account_connect_async      (ChattyPpAccount *self, */
/*                                                        GCancellable    *cancellable, */
/*                                                        GAsyncReadyCallback callback, */
/*                                                        gpointer         user_data); */
/* gboolean         chatty_pp_account_connect_finish     (ChattyPpAccount *self, */
/*                                                        GAsyncResult    *result, */
/*                                                        GError         **error); */

G_END_DECLS
