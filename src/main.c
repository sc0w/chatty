/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include <glib/gi18n.h>

#include "chatty-config.h"
#include "chatty-window.h"
#include "chatty-purple-init.h"


int
main (int   argc,
      char *argv[])
{
  GtkApplication *app;
  int status;

  g_set_prgname (CHATTY_APP_ID);
  g_set_application_name (CHATTY_APP_NAME);

  app = gtk_application_new (CHATTY_APP_ID, G_APPLICATION_FLAGS_NONE);
  g_signal_connect (app, "activate", G_CALLBACK (chatty_window_activate), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}
