/* chatty-application.c
 *
 * Copyright 2019 Purism SPC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-application"

#include <glib/gi18n.h>

#include "chatty-config.h"
#include "chatty-window.h"
#include "chatty-application.h"
#include "chatty-purple-init.h"
#include "chatty-buddy-list.h"

/**
 * SECTION: chatty-application
 * @title: ChattyApplication
 * @short_description: Base Application class
 * @include: "chatty-application.h"
 */

struct _ChattyApplication
{
  GtkApplication  parent_instance;
  gboolean        daemon;
  GtkCssProvider *css_provider;
};

G_DEFINE_TYPE (ChattyApplication, chatty_application, GTK_TYPE_APPLICATION)

static GOptionEntry cmd_options[] = {
  { "version", 'v', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, N_("Show release version"), NULL },
  { "daemon", 'D', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, N_("Start in daemon mode"), NULL },
  { "nologin", 'n', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, N_("Disable all accounts"), NULL },
  { "debug", 'd', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, N_("Enable libpurple debug messages"), NULL },
  { "verbose", 'V', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, N_("Enable verbose libpurple debug messages"), NULL },
  { NULL }
};

static void
chatty_application_finalize (GObject *object)
{
  ChattyApplication *self = (ChattyApplication *)object;

  g_clear_object (&self->css_provider);

  G_OBJECT_CLASS (chatty_application_parent_class)->finalize (object);
}

static gint
chatty_application_handle_local_options (GApplication *application,
                                         GVariantDict *options)
{
  g_autoptr(GError) error = NULL;
  gboolean          result;

  ChattyApplication *self = (ChattyApplication *)application;

  chatty_data_t *chatty = chatty_get_data ();

  chatty->cml_options = CHATTY_CML_OPT_NONE;

  result = g_application_register (application, NULL, &error);

  if (!result) {
    g_debug ("Application register failed: %s", error->message);
  }

  if (g_variant_dict_contains (options, "daemon")) {
    if (chatty->app_running == FALSE) {
      self->daemon = TRUE;
    } else {
      g_debug ("Daemon mode not possible, application is already running");
    }
  } else if (g_variant_dict_contains (options, "nologin")) {
    chatty->cml_options |= CHATTY_CML_OPT_DISABLE;
  } else if (g_variant_dict_contains (options, "debug")) {
    chatty->cml_options |= CHATTY_CML_OPT_DEBUG;
  } else if (g_variant_dict_contains (options, "verbose")) {
    chatty->cml_options |= CHATTY_CML_OPT_VERBOSE;
  } else if (g_variant_dict_contains (options, "version")) {
    g_print ("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
    return 0;
  }

  return -1;
}


static void
chatty_application_startup (GApplication *application)
{
  ChattyApplication *self = (ChattyApplication *)application;

  chatty_data_t *chatty = chatty_get_data ();

  self->daemon = FALSE;

  chatty->uri = NULL;
  chatty->app_running = FALSE;

  G_APPLICATION_CLASS (chatty_application_parent_class)->startup (application);

  g_set_application_name (CHATTY_APP_NAME);

  self->css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (self->css_provider,
                                       "/sm/puri/chatty/css/style.css");

  gtk_style_context_add_provider_for_screen (gdk_screen_get_default(),
                                             GTK_STYLE_PROVIDER (self->css_provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_USER);
}


static void
chatty_application_activate (GApplication *application)
{
  GtkApplication *app = (GtkApplication *)application;
  GtkWindow      *window;
  gboolean        show_win;

  ChattyApplication *self = (ChattyApplication *)application;

  g_assert (GTK_IS_APPLICATION (app));

  window = gtk_application_get_active_window (app);

  if (window) {
    show_win = TRUE;
  } else {
    chatty_window_activate (app, NULL);

    show_win = !self->daemon;
  }

  if (show_win) {
    window = gtk_application_get_active_window (app);

    gtk_window_present (window);
  }
}


static void
chatty_application_open (GApplication  *application,
                         GFile        **files,
                         gint           n_files,
                         const gchar   *hint)
{
  gint i;

  chatty_data_t *chatty = chatty_get_data ();

  for (i = 0; i < n_files; i++) {
    char *uri = g_file_get_uri (files[i]);

    if (g_file_has_uri_scheme (files[i], "sms")) {
      uri = g_file_get_uri (files[i]);

      if (chatty->app_running) {
        chatty_blist_add_buddy_from_uri (uri);
      } else {
        chatty->uri = g_strdup (uri);
      }

      g_free (uri);
    }
  }

  g_application_activate (application);
}


static void
chatty_application_class_init (ChattyApplicationClass *klass)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = chatty_application_finalize;

  application_class->handle_local_options = chatty_application_handle_local_options;
  application_class->startup = chatty_application_startup;
  application_class->activate = chatty_application_activate;
  application_class->open = chatty_application_open;
}


static void
chatty_application_init (ChattyApplication *self)
{
  g_application_add_main_option_entries (G_APPLICATION (self), cmd_options);
}


ChattyApplication *
chatty_application_new (void)
{
  return g_object_new (CHATTY_TYPE_APPLICATION,
                       "application-id", CHATTY_APP_ID,
                       "flags", G_APPLICATION_HANDLES_OPEN,
                       "register-session", TRUE,
                       NULL);
}
