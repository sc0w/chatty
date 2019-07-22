/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include <glib.h>
#include "chatty-utils.h"

char *
chatty_utils_jabber_id_strip (const char *name)
{
  char ** split;
  char *  stripped;

  split = g_strsplit (name, "/", -1);
  stripped = g_strdup (split[0]);

  g_strfreev (split);

  return stripped;
}

void
chatty_utils_generate_uuid(char **uuid){
  unsigned long int number;
  int written = 0;

  *uuid = g_malloc0(60 * sizeof(char));

  for(int i=0; i<4; i++){
    number = random();
    written += sprintf(*uuid+written, "%010ld", number);
    if (i<3){
      written += sprintf(*uuid+written, "-");
    }
  }

}
