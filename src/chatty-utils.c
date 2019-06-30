/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include <glib.h>
#include <time.h>
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

char *
chatty_utils_generate_uuid(void){
  char *result;
  unsigned long int number;
  int written = 0;

  result = g_malloc0(60 * sizeof(char));
  srand(time(NULL));

  for(int i=0; i<4; i++){
    number = random();
    written += sprintf(result+written, "%010ld", number);
    if (i<3){
      written += sprintf(result+written, "-");
    }
  }

  return result;
}
