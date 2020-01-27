/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __LURCH_H_INCLUDE__
#define __LURCH_H_INCLUDE__


enum {
  LURCH_FP_OWN_DEVICE,
  LURCH_FP_CONTACTS_DEVICE
} e_lurch_fp_type;


enum {
  LURCH_STATUS_DISABLED = 0,  // manually disabled
  LURCH_STATUS_NOT_SUPPORTED, // no OMEMO support, i.e. there is no devicelist node
  LURCH_STATUS_NO_SESSION,    // OMEMO is supported, but there is no libsignal session yet
  LURCH_STATUS_OK             // OMEMO is supported and session exists
} e_lurch_status;


void chatty_lurch_get_status (PurpleConversation *conv);
void chatty_lurch_fp_device_get (PurpleConversation *conv);
void chatty_lurch_get_fp_list_own (PurpleAccount *account);
GtkWidget* chatty_lurch_create_fingerprint_row (const char *fp, guint id);

#endif
