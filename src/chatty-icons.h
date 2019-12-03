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

typedef enum
{
   CHATTY_ICON_COLOR_GREY,
   CHATTY_ICON_COLOR_GREEN,
   CHATTY_ICON_COLOR_BLUE,
   CHATTY_ICON_COLOR_PURPLE
} ChattyPurpleIconColor;

void chatty_icon_do_alphashift (GdkPixbuf *pixbuf, int shift);
GdkPixbuf *chatty_icon_shape_pixbuf_circular (GdkPixbuf *pixbuf);
GIcon *chatty_icon_get_gicon_from_pixbuf (GdkPixbuf *pixbuf);
GdkPixbuf *chatty_icon_pixbuf_from_data (const guchar *buf, gsize count);

gpointer chatty_icon_get_data_from_pixbuf (const char               *file_name,
                                           PurplePluginProtocolInfo *prpl_info,
                                           size_t                   *len);

GdkPixbuf *chatty_icon_get_buddy_icon (PurpleBlistNode *node,
                                       const char      *name,
                                       guint            size,
                                       const char      *color,
                                       gboolean         greyed);

#endif
