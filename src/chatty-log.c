/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-log.c
 *
 * Copyright 2021 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib.h>
#include <stdio.h>

#include "chatty-log.h"

char *domain;
static int verbosity;
gboolean any_domain;


static GLogWriterOutput
chatty_log_write (GLogLevelFlags   log_level,
               const char      *log_domain,
               const GLogField *fields,
               gsize            n_fields,
               gpointer         user_data)
{
  if (log_level == G_LOG_LEVEL_MESSAGE ||
      log_level == G_LOG_LEVEL_INFO ||
      log_level == CHATTY_LOG_LEVEL_TRACE)
    {
      g_autofree char *message = NULL;
      char *end;

      message = g_log_writer_format_fields (log_level, fields, n_fields,
                                            g_log_writer_supports_color (fileno (stdout)));
      end = strstr (message, "LOG-");

      if (end)
        {
          end = end + strlen ("LOG-");
          memcpy (end, "TRACE", strlen ("TRACE"));
        }

      fprintf (stdout, "%s\n", message);
      fflush (stdout);

      return G_LOG_WRITER_HANDLED;
    }

  return g_log_writer_standard_streams (log_level, fields, n_fields, user_data);
}

static GLogWriterOutput
chatty_log_handler (GLogLevelFlags   log_level,
                 const GLogField *fields,
                 gsize            n_fields,
                 gpointer         user_data)
{
  const char *log_domain = NULL;

  /* If domain is “all” show logs upto debug regardless of the verbosity */
  if (any_domain && domain && log_level <= G_LOG_LEVEL_DEBUG)
    return chatty_log_write (log_level, log_domain, fields, n_fields, user_data);

  switch ((int)log_level)
    {
    case G_LOG_LEVEL_MESSAGE:
      if (verbosity < 1)
        return G_LOG_WRITER_HANDLED;
      break;

    case G_LOG_LEVEL_INFO:
      if (verbosity < 2)
        return G_LOG_WRITER_HANDLED;
      break;

    case G_LOG_LEVEL_DEBUG:
      if (verbosity < 3)
        return G_LOG_WRITER_HANDLED;
      break;

    case CHATTY_LOG_LEVEL_TRACE:
      if (verbosity < 4)
      return G_LOG_WRITER_HANDLED;
      break;

    default:
      break;
    }

  for (guint i = 0; log_domain == NULL && i < n_fields; i++)
    {
      const GLogField *field = &fields[i];

      if (g_strcmp0 (field->key, "GLIB_DOMAIN") == 0)
        log_domain = field->value;
    }

  /* GdkPixbuf logs are too much verbose, skip unless asked not to. */
  if (g_strcmp0 (log_domain, "GdkPixbuf") == 0 &&
      g_strcmp0 (log_domain, domain) != 0)
    return G_LOG_WRITER_HANDLED;

  if (any_domain)
    return chatty_log_write (log_level, log_domain, fields, n_fields, user_data);

  if (any_domain || !domain || !log_domain)
    return G_LOG_WRITER_HANDLED;

  if (strstr (log_domain, domain))
    chatty_log_write (log_level, log_domain, fields, n_fields, user_data);

  return G_LOG_WRITER_HANDLED;
}

void
chatty_log_init (void)
{
  static gsize initialized = 0;

  if (g_once_init_enter (&initialized))
    {
      domain = g_strdup (g_getenv ("G_MESSAGES_DEBUG"));

      if (!domain || g_str_equal (domain, "all"))
        any_domain = TRUE;

      g_log_set_writer_func (chatty_log_handler, NULL, NULL);
      g_once_init_leave (&initialized, 1);
    }
}

void
chatty_log_finalize (void)
{
  g_clear_pointer (&domain, g_free);
}

void
chatty_log_increase_verbosity (void)
{
  verbosity++;
}

int
chatty_log_get_verbosity (void)
{
  return verbosity;
}
