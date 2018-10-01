/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */


#ifndef __ICON_H_INCLUDE__
#define __ICON_H_INCLUDE__

#include "purple.h"

typedef enum
{
   CHATTY_PRPL_ICON_SMALL = 1,
   CHATTY_PRPL_ICON_MEDIUM,
   CHATTY_PRPL_ICON_LARGE
} ChattyPurpleIconSize;

GdkPixbuf *
chatty_create_prpl_icon (PurpleAccount *account, ChattyPurpleIconSize size);

GdkPixbuf *
chatty_icon_get_buddy_icon (PurpleBlistNode *node,
                            guint           scale,
                            gboolean        greyed);

void chatty_icon_do_alphashift (GdkPixbuf *pixbuf, int shift);

#endif
