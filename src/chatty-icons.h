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
   CHATTY_ICON_SIZE_SMALL = 1,
   CHATTY_ICON_SIZE_MEDIUM,
   CHATTY_ICON_SIZE_LARGE
} ChattyPurpleIconSize;

typedef enum
{
   CHATTY_ICON_COLOR_GREY,
   CHATTY_ICON_COLOR_GREEN,
   CHATTY_ICON_COLOR_BLUE,
   CHATTY_ICON_COLOR_PURPLE
} ChattyPurpleIconColor;

GdkPixbuf *
chatty_icon_create_prpl_icon (PurpleAccount        *account,
                              ChattyPurpleIconSize size);

GdkPixbuf *
chatty_icon_get_buddy_icon (PurpleBlistNode *node,
                            const char      *name,
                            guint            scale,
                            const char      *color,
                            gboolean         greyed);

void chatty_icon_do_alphashift (GdkPixbuf *pixbuf, int shift);
GtkWidget *chatty_icon_get_avatar_button (int size);

#endif
