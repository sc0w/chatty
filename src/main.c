/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include <glib/gi18n.h>
#include "chatty-config.h"
#include "chatty-application.h"
#include "chatty-purple-init.h"


int
main (int   argc,
      char *argv[])
{
  g_autoptr(ChattyApplication) application = NULL;

  g_set_prgname (CHATTY_APP_ID);
  application = chatty_application_new ();

  return g_application_run (G_APPLICATION (application), argc, argv);
}
