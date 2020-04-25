/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include <time.h>
#include <glib/gi18n.h>
#include "chatty-config.h"
#include "chatty-application.h"
#include "chatty-manager.h"
#include "chatty-history.h"
#include "chatty-settings.h"

#define LIBFEEDBACK_USE_UNSTABLE_API
#include <libfeedback.h>


int
main (int   argc,
      char *argv[])
{
  int status;

  g_autoptr(ChattyApplication) application = NULL;

	textdomain (GETTEXT_PACKAGE);
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  g_set_prgname (CHATTY_APP_ID);
  lfb_init (CHATTY_APP_ID, NULL);
  application = chatty_application_new ();

  chatty_history_open();

  status = g_application_run (G_APPLICATION (application), argc, argv);

  g_object_unref (chatty_settings_get_default ());
  chatty_history_close();

  lfb_uninit();
  return status;
}
