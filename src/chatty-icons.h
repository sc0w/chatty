/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __ICON_H_INCLUDE__
#define __ICON_H_INCLUDE__

#include "purple.h"

typedef enum
{
   CHATTY_ICON_SIZE_SMALL  = 28,
   CHATTY_ICON_SIZE_MEDIUM = 36,
   CHATTY_ICON_SIZE_LARGE  = 96
} ChattyPurpleIconSize;

void chatty_icon_do_alphashift (GdkPixbuf *pixbuf, int shift);
GdkPixbuf *chatty_icon_shape_pixbuf_circular (GdkPixbuf *pixbuf);
GdkPixbuf *chatty_icon_pixbuf_from_data (const guchar *buf, gsize count);

GdkPixbuf *chatty_icon_get_buddy_icon (PurpleBlistNode *node);

#endif
