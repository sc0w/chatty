/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include <time.h>
#include <glib/gi18n.h>
#include "chatty-config.h"
#include "chatty-application.h"
#include "chatty-purple-init.h"
#include "chatty-history.h"
#include <libnotify/notify.h>


int
main (int   argc,
      char *argv[])
{
  int status;

  g_autoptr(ChattyApplication) application = NULL;

  notify_init ("Chatty");

  g_set_prgname (CHATTY_APP_ID);
  application = chatty_application_new ();

  srand(time(NULL));
  chatty_history_open();

  status = g_application_run (G_APPLICATION (application), argc, argv);

  chatty_history_close();

  return status;
}
