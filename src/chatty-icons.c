/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include <glib.h>
#include <gtk/gtk.h>
#include <math.h>
#include "purple.h"
#include "chatty-icons.h"


static GObject *
chatty_icon_pixbuf_from_data_helper (const guchar *buf,
                                     gsize        count,
                                     gboolean     animated)
{
  GObject *pixbuf;
  GdkPixbufLoader *loader;
  GError *error = NULL;

  loader = gdk_pixbuf_loader_new ();

  if (!gdk_pixbuf_loader_write (loader, buf, count, &error) || error) {
    purple_debug_warning ("gtkutils", "gdk_pixbuf_loader_write() "
                          "failed with size=%zu: %s\n", count,
                          error ? error->message : "(no error message)");

    if (error) {
      g_error_free (error);
    }

    g_object_unref (G_OBJECT(loader));

    return NULL;
  }

  if (!gdk_pixbuf_loader_close(loader, &error) || error) {
    purple_debug_warning ("gtkutils", "gdk_pixbuf_loader_close() "
                          "failed for image of size %zu: %s\n", count,
                          error ? error->message : "(no error message)");

    if (error) {
      g_error_free(error);
    }

    g_object_unref(G_OBJECT(loader));

    return NULL;
  }

  if (animated) {
    pixbuf = G_OBJECT(gdk_pixbuf_loader_get_animation (loader));
  } else {
    pixbuf = G_OBJECT(gdk_pixbuf_loader_get_pixbuf (loader));
  }

  if (!pixbuf) {
    purple_debug_warning("gtkutils", "%s() returned NULL for image "
                         "of size %zu\n",
                         animated ? "gdk_pixbuf_loader_get_animation"
                        : "gdk_pixbuf_loader_get_pixbuf", count);

    g_object_unref(G_OBJECT(loader));

    return NULL;
  }

  g_object_ref(pixbuf);
  g_object_unref(G_OBJECT(loader));

  return pixbuf;
}


static GdkPixbuf *
chatty_icon_pixbuf_from_data (const guchar *buf, gsize count)
{
  return GDK_PIXBUF (chatty_icon_pixbuf_from_data_helper (buf, count, FALSE));
}


GtkWidget *
chatty_icon_get_avatar_button (int size)
{
  GtkWidget       *image;
  GtkWidget       *button_avatar;
  GtkStyleContext *sc;

  button_avatar = gtk_menu_button_new ();

  gtk_widget_set_hexpand (button_avatar, FALSE);
  gtk_widget_set_halign (button_avatar, GTK_ALIGN_CENTER);
  gtk_widget_set_size_request (GTK_WIDGET(button_avatar), size, size);

  image = gtk_image_new_from_icon_name ("avatar-default-symbolic", GTK_ICON_SIZE_DIALOG);

  gtk_button_set_image (GTK_BUTTON (button_avatar), image);
  sc = gtk_widget_get_style_context (button_avatar);
  gtk_style_context_add_class (sc, "button_avatar");

  return button_avatar;
}


GdkPixbuf *
chatty_icon_get_buddy_icon (PurpleBlistNode *node,
                            guint           scale,
                            gboolean        greyed)
{
  gsize                     len;
  PurpleBuddy               *buddy = NULL;
  PurpleGroup               *group = NULL;
  const guchar              *data = NULL;
  GdkPixbuf                 *buf, *ret = NULL;
  PurpleBuddyIcon           *icon = NULL;
  PurpleAccount             *account = NULL;
  PurpleContact             *contact = NULL;
  PurpleStoredImage         *custom_img;
  PurplePluginProtocolInfo  *prpl_info = NULL;
  gint                      orig_width,
                            orig_height,
                            scale_width,
                            scale_height;
  float                     scale_size;

  if (PURPLE_BLIST_NODE_IS_CONTACT (node)) {
    buddy = purple_contact_get_priority_buddy((PurpleContact*)node);
    contact = (PurpleContact*)node;
  } else if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
    buddy = (PurpleBuddy*)node;
    contact = purple_buddy_get_contact(buddy);
  } else if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
    group = (PurpleGroup*)node;
  } else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
    /* We don't need to do anything here. We just need to not fall
     * into the else block and return. */
  } else {
    return NULL;
  }

  if (buddy) {
    account = purple_buddy_get_account (buddy);
  }

  if(account && account->gc) {
    prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(account->gc->prpl);
  }

  if (contact) {
    custom_img = purple_buddy_icons_node_find_custom_icon ((PurpleBlistNode*)contact);
  } else {
    custom_img = purple_buddy_icons_node_find_custom_icon (node);
  }

  if (custom_img) {
    data = purple_imgstore_get_data (custom_img);
    len = purple_imgstore_get_size (custom_img);
  }

  if (data == NULL && buddy) {
    icon = purple_buddy_icons_find (buddy->account, buddy->name);
    if (icon) {
      data = purple_buddy_icon_get_data (icon, &len);
    }
  }

  if (data != NULL) {
    buf = chatty_icon_pixbuf_from_data (data, len);
    purple_buddy_icon_unref (icon);
  } else {
    GtkIconTheme *icon_theme;

    icon_theme = gtk_icon_theme_get_default ();

    buf = gtk_icon_theme_load_icon (icon_theme,
                                    "avatar-default-symbolic",
                                    48,
                                    0,
                                    NULL);

    if (!buf) {
      return NULL;
    }
  }

  purple_imgstore_unref (custom_img);

  if (greyed) {
    gboolean offline = FALSE, idle = FALSE;

    if (buddy) {
      PurplePresence *presence = purple_buddy_get_presence(buddy);
      if (!PURPLE_BUDDY_IS_ONLINE(buddy))
        offline = TRUE;
      if (purple_presence_is_idle(presence))
        idle = TRUE;
    } else if (group) {
      if (purple_blist_get_group_online_count (group) == 0)
        offline = TRUE;
    }

    if (offline) {
      gdk_pixbuf_saturate_and_pixelate (buf, buf, 0.0, FALSE);
    }

    if (idle) {
      gdk_pixbuf_saturate_and_pixelate (buf, buf, 0.25, FALSE);
    }
  }

  scale_width = orig_width = gdk_pixbuf_get_width (buf);
  scale_height = orig_height = gdk_pixbuf_get_height (buf);

  if (prpl_info &&
      prpl_info->icon_spec.scale_rules & PURPLE_ICON_SCALE_DISPLAY) {
    purple_buddy_icon_get_scale_size (&prpl_info->icon_spec,
                                      &scale_width,
                                      &scale_height);
  }

  scale_size = 16.0 * (float)scale;

  if (scale) {
    GdkPixbuf *tmpbuf;

    if (scale_height > scale_width) {
      scale_width = scale_size * (double)scale_width / (double)scale_height;
      scale_height = scale_size;
    } else {
      scale_height = scale_size * (double)scale_height / (double)scale_width;
      scale_width = scale_size;
    }

    tmpbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, scale_width, scale_height);
    gdk_pixbuf_fill (tmpbuf, 0x00000000);
    gdk_pixbuf_scale (buf, tmpbuf, 0, 0,
                      scale_width, scale_height,
                      0, 0,
                      (double)scale_width / (double)orig_width,
                      (double)scale_height / (double)orig_height,
                      GDK_INTERP_BILINEAR);

    ret = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, scale_size, scale_size);
    gdk_pixbuf_fill (ret, 0x00000000);
    gdk_pixbuf_copy_area (tmpbuf,
                          0, 0,
                          scale_width, scale_height,
                          ret,
                          (scale_size - scale_width) / 2,
                          (scale_size - scale_height) / 2);

    g_object_unref (G_OBJECT(tmpbuf));
  } else {
    ret = gdk_pixbuf_scale_simple (buf,
                                   scale_width,
                                   scale_height,
                                   GDK_INTERP_BILINEAR);
  }

  cairo_format_t  format;
  cairo_surface_t *surface;
  cairo_t         *cr;

  format = (gdk_pixbuf_get_has_alpha (ret)) ?
    CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;

  surface = cairo_image_surface_create (format, scale_size, scale_size);
  g_assert (surface != NULL);
  cr = cairo_create (surface);

  // create a grey background for the default avatar and
  // for buddy icons which don't have square format
  cairo_set_source_rgb (cr, 0.7, 0.7, 0.7);
  cairo_arc (cr,
             scale_size / 2,
             scale_size / 2,
             scale_size / 2,
             0,
             2 * M_PI);

  cairo_fill (cr);

  gdk_cairo_set_source_pixbuf (cr, ret, 0, 0);

  cairo_arc (cr,
             scale_size / 2,
             scale_size / 2,
             scale_size / 2,
             0,
             2 * M_PI);

  cairo_clip (cr);
  cairo_paint (cr);

  ret = gdk_pixbuf_get_from_surface (surface,
                                     0,
                                     0,
                                     scale_size,
                                     scale_size);

  cairo_surface_destroy (surface);
  cairo_destroy (cr);

  g_object_unref(G_OBJECT(buf));

  return ret;
}


/* Altered from do_colorshift in gnome-panel */
void
chatty_icon_do_alphashift (GdkPixbuf *pixbuf,
                           int       shift)
{
  gint   i, j;
  gint   width, height, padding;
  guchar *pixels;
  int    val;

  if (!gdk_pixbuf_get_has_alpha(pixbuf))
    return;

  width = gdk_pixbuf_get_width(pixbuf);
  height = gdk_pixbuf_get_height(pixbuf);
  padding = gdk_pixbuf_get_rowstride(pixbuf) - width * 4;
  pixels = gdk_pixbuf_get_pixels(pixbuf);

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
