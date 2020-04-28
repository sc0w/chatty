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
#include "chatty-utils.h"
#include "users/chatty-pp-account.h"
#include "chatty-manager.h"
#include "chatty-application.h"
#include "chatty-settings.h"

/**
 * SECTION: chatty-application
 * @title: ChattyApplication
 * @short_description: Base Application class
 * @include: "chatty-application.h"
 */

struct _ChattyApplication
{
  GtkApplication  parent_instance;

  GtkWidget      *main_window;
  ChattySettings *settings;
  GtkCssProvider *css_provider;

  char *uri;
  guint open_uri_id;

  gboolean daemon;
  gboolean enable_debug;
  gboolean enable_verbose;
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

static gboolean
application_open_uri (ChattyApplication *self)
{
  g_clear_handle_id (&self->open_uri_id, g_source_remove);

  if (self->main_window && self->uri)
    chatty_window_set_uri (CHATTY_WINDOW (self->main_window), self->uri);

  g_clear_pointer (&self->uri, g_free);

  return G_SOURCE_REMOVE;
}

static void
chatty_application_finalize (GObject *object)
{
  ChattyApplication *self = (ChattyApplication *)object;

  g_clear_handle_id (&self->open_uri_id, g_source_remove);
  g_clear_object (&self->css_provider);

  G_OBJECT_CLASS (chatty_application_parent_class)->finalize (object);
}

static gint
chatty_application_handle_local_options (GApplication *application,
                                         GVariantDict *options)
{
  if (g_variant_dict_contains (options, "version")) {
    g_print ("%s %s\n", PACKAGE_NAME, GIT_VERSION);
    return 0;
  }

  return -1;
}

static gint
chatty_application_command_line (GApplication            *application,
                                 GApplicationCommandLine *command_line)
{
  ChattyApplication *self = (ChattyApplication *)application;
  GVariantDict  *options;
  g_auto(GStrv) arguments = NULL;
  gint argc;

  options = g_application_command_line_get_options_dict (command_line);

  if (g_variant_dict_contains (options, "daemon")) {
    if (!g_application_command_line_get_is_remote (command_line)) {
      self->daemon = TRUE;
      g_application_hold (application);
    } else {
      g_debug ("Daemon mode not possible, application is already running");
    }
  }

  if (g_variant_dict_contains (options, "nologin")) {
    chatty_manager_disable_auto_login (chatty_manager_get_default (), TRUE);
  } else if (g_variant_dict_contains (options, "debug")) {
    self->enable_debug = TRUE;
  } else if (g_variant_dict_contains (options, "verbose")) {
    self->enable_debug = TRUE;
    self->enable_verbose = TRUE;
  }

  purple_debug_set_enabled (self->enable_debug);
  purple_debug_set_verbose (self->enable_verbose);

  arguments = g_application_command_line_get_arguments (command_line, &argc);

  /* Keep only the last URI, if there are many */
  for (guint i = 0; i < argc; i++)
    if (g_str_has_prefix (arguments[i], "sms:")) {
      g_free (self->uri);
      self->uri = g_strdup (arguments[i]);
    }

  g_application_activate (application);

  return 0;
}

static void
chatty_application_startup (GApplication *application)
{
  ChattyApplication *self = (ChattyApplication *)application;

  self->daemon = FALSE;

  chatty_manager_get_default ();

  G_APPLICATION_CLASS (chatty_application_parent_class)->startup (application);

  g_set_application_name (CHATTY_APP_NAME);

  self->settings = chatty_settings_get_default ();

  self->css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (self->css_provider,
                                       "/sm/puri/chatty/css/style.css");

  gtk_style_context_add_provider_for_screen (gdk_screen_get_default(),
                                             GTK_STYLE_PROVIDER (self->css_provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_USER);
  chatty_manager_purple (chatty_manager_get_default ());
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
    self->main_window = chatty_window_new (app);

    g_object_add_weak_pointer (G_OBJECT (self->main_window), (gpointer *)&self->main_window);
    show_win = !self->daemon;

    if (self->daemon)
      g_signal_connect (G_OBJECT (self->main_window),
                        "delete-event",
                        G_CALLBACK (gtk_widget_hide_on_delete),
                        NULL);
  }

  if (show_win) {
    window = gtk_application_get_active_window (app);

    gtk_window_present (window);
  }

  /* Open with some delay so that the modem is ready when not in daemon mode */
  if (self->main_window && self->uri)
    self->open_uri_id = g_timeout_add (100,
                                       G_SOURCE_FUNC (application_open_uri),
                                       self);
}


static void
chatty_application_class_init (ChattyApplicationClass *klass)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = chatty_application_finalize;

  application_class->handle_local_options = chatty_application_handle_local_options;
  application_class->command_line = chatty_application_command_line;
  application_class->startup = chatty_application_startup;
  application_class->activate = chatty_application_activate;
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
                       "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
                       "register-session", TRUE,
                       NULL);
}
