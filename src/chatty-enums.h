/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-enums.h
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once


/**
 * ChattyStatus:
 *
 * Account connection status for #ChattyPpAccount
 */
typedef enum
{
  CHATTY_DISCONNECTED,
  CHATTY_CONNECTING,
  CHATTY_CONNECTED,
} ChattyStatus;
