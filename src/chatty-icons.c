/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-icons"

#include <glib.h>
#include <gtk/gtk.h>
#include <math.h>
#include "purple.h"
#include "chatty-icons.h"


GdkPixbuf *
chatty_icon_pixbuf_from_data (const guchar *buf,
                              gsize         count)
{
  GdkPixbuf       *pixbuf;
  g_autoptr(GdkPixbufLoader) loader = NULL;
  g_autoptr(GError) error = NULL;

  loader = gdk_pixbuf_loader_new ();

  if (!gdk_pixbuf_loader_write (loader, buf, count, &error)) {
    g_debug ("%s: pixbuf_loder_write failed: %s", __func__, error->message);
    return NULL;
  }

  if (!gdk_pixbuf_loader_close (loader, &error)) {
    g_debug ("%s: pixbuf_loder_close failed: %s", __func__, error->message);
    return NULL;
  }

  pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

  if (!pixbuf) {
    g_debug ("%s: pixbuf creation failed", __func__);
    return NULL;
  }

  return g_object_ref (pixbuf);
}


GdkPixbuf *
chatty_icon_shape_pixbuf_circular (GdkPixbuf *pixbuf)
{
  cairo_format_t   format;
  cairo_surface_t *surface;
  cairo_t         *cr;
  GdkPixbuf       *ret;
  int              width, height, size;

  format = CAIRO_FORMAT_ARGB32;

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  size = (width >= height) ? width : height;

  surface = cairo_image_surface_create (format, size, size);

  cr = cairo_create (surface);

  cairo_set_antialias (cr, CAIRO_ANTIALIAS_SUBPIXEL); 

  cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);

  cairo_arc (cr,
             size / 2,
             size / 2,
             size / 2,
             0,
             2 * M_PI);

  cairo_fill (cr);

  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);

  cairo_arc (cr,
             size / 2,
             size / 2,
             size / 2,
             0,
             2 * M_PI);

  cairo_clip (cr);
  cairo_paint (cr);

  ret = gdk_pixbuf_get_from_surface (surface,
                                     0,
                                     0,
                                     size,
                                     size);

  cairo_surface_destroy (surface);
  cairo_destroy (cr);

  return ret;
}


GdkPixbuf *
chatty_icon_get_buddy_icon (PurpleBlistNode *node)
{
  gsize                      len;
  const guchar              *data = NULL;
  GdkPixbuf                 *buf = NULL;
  PurpleBuddyIcon           *icon = NULL;
  PurpleStoredImage         *custom_img;

  if (!node || !PURPLE_BLIST_NODE_IS_CHAT (node))
    return NULL;

  custom_img = purple_buddy_icons_node_find_custom_icon (node);

  if (custom_img) {
    data = purple_imgstore_get_data (custom_img);
    len = purple_imgstore_get_size (custom_img);
  }

  purple_imgstore_unref (custom_img);

  if (data != NULL) {
    buf = chatty_icon_pixbuf_from_data (data, len);
    purple_buddy_icon_unref (icon);

    return buf;
  }

  return NULL;
}

/* Altered from do_colorshift in gnome-panel */
void
chatty_icon_do_alphashift (GdkPixbuf *pixbuf,
                           int        shift)
{
  gint    i, j;
  gint    width, height, padding;
  guchar *pixels;
  int     val;

  if (!gdk_pixbuf_get_has_alpha (pixbuf)) {
    return;
  }

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);
  padding = gdk_pixbuf_get_rowstride (pixbuf) - width * 4;
  pixels = gdk_pixbuf_get_pixels (pixbuf);

  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      pixels++;
      pixels++;
      pixels++;
      val = *pixels - shift;
      *(pixels++) = CLAMP(val, 0, 255);
    }

    pixels += padding;
  }
}
