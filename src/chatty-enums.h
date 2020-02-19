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

/**
 * ChattyProtocol:
 *
 * Protocols supported/implemented by #ChattyUser
 */
typedef enum
{
  CHATTY_PROTOCOL_NONE     = 0,
  CHATTY_PROTOCOL_SMS      = 1 << 0,
  CHATTY_PROTOCOL_MMS      = 1 << 1,
  CHATTY_PROTOCOL_XMPP     = 1 << 2,
  CHATTY_PROTOCOL_MATRIX   = 1 << 3,
  CHATTY_PROTOCOL_TELEGRAM = 1 << 4,
  CHATTY_PROTOCOL_DELTA    = 1 << 5, /* prpl-delta */
  CHATTY_PROTOCOL_THREEPL  = 1 << 6, /* prpl-threepl */
  CHATTY_PROTOCOL_ANY      = ~0
} ChattyProtocol;
