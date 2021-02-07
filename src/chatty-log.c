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
#include <unistd.h>

#include "chatty-log.h"

char *domain;
static int verbosity;
gboolean any_domain;


static const char *
get_log_level_prefix (GLogLevelFlags log_level,
                      gboolean       use_color)
{
  if (use_color)
    {
      switch ((int)log_level)        /* Same colors as used in GLib */
        {
        case G_LOG_LEVEL_ERROR:       return "   \033[1;31mERROR\033[0m";
        case G_LOG_LEVEL_CRITICAL:    return "\033[1;35mCRITICAL\033[0m";
        case G_LOG_LEVEL_WARNING:     return " \033[1;33mWARNING\033[0m";
        case G_LOG_LEVEL_MESSAGE:     return " \033[1;32mMESSAGE\033[0m";
        case G_LOG_LEVEL_INFO:        return "    \033[1;32mINFO\033[0m";
        case G_LOG_LEVEL_DEBUG:       return "   \033[1;32mDEBUG\033[0m";
        case CHATTY_LOG_LEVEL_TRACE:  return "   \033[1;36mTRACE\033[0m";
        default:                      return " UNKNOWN";
        }
    }
  else
    {
      switch ((int)log_level)
        {
        case G_LOG_LEVEL_ERROR:      return "   ERROR";
        case G_LOG_LEVEL_CRITICAL:   return "CRITICAL";
        case G_LOG_LEVEL_WARNING:    return " WARNING";
        case G_LOG_LEVEL_MESSAGE:    return " MESSAGE";
        case G_LOG_LEVEL_INFO:       return "    INFO";
        case G_LOG_LEVEL_DEBUG:      return "   DEBUG";
        case CHATTY_LOG_LEVEL_TRACE: return "   TRACE";
        default:                     return " UNKNOWN";
        }
    }
}

static GLogWriterOutput
chatty_log_write (GLogLevelFlags   log_level,
                  const char      *log_domain,
                  const char      *log_message,
                  const GLogField *fields,
                  gsize            n_fields,
                  gpointer         user_data)
{
  g_autoptr(GString) log_str = NULL;
  FILE *stream;
  gboolean can_color;

  if (log_level & (G_LOG_LEVEL_ERROR |
                   G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING))
    stream = stderr;
  else
    stream = stdout;

  log_str = g_string_new (NULL);

  /* Add local time */
  {
    char buffer[32];
    struct tm tm_now;
    time_t sec_now;
    gint64 now;

    now = g_get_real_time ();
    sec_now = now / G_USEC_PER_SEC;
    tm_now = *localtime (&sec_now);
    strftime (buffer, sizeof (buffer), "%H:%M:%S", &tm_now);

    g_string_append_printf (log_str, "%s.%04d ", buffer,
                            (int)((now % G_USEC_PER_SEC) / 100));
  }

  g_string_append_printf (log_str, "%20s[%5d]:", log_domain, getpid ());

  can_color = g_log_writer_supports_color (fileno (stream));
  g_string_append_printf (log_str, "%s: ", get_log_level_prefix (log_level, can_color));
  g_string_append (log_str, log_message);

  fprintf (stream, "%s\n", log_str->str);
  fflush (stream);

  return G_LOG_WRITER_HANDLED;
}

static GLogWriterOutput
chatty_log_handler (GLogLevelFlags   log_level,
                    const GLogField *fields,
                    gsize            n_fields,
                    gpointer         user_data)
{
  const char *log_domain = NULL;
  const char *log_message = NULL;

  /* If domain is “all” show logs upto debug regardless of the verbosity */
  switch ((int)log_level)
    {
    case G_LOG_LEVEL_MESSAGE:
      if (any_domain && domain)
        break;
      if (verbosity < 1)
        return G_LOG_WRITER_HANDLED;
      break;

    case G_LOG_LEVEL_INFO:
      if (any_domain && domain)
        break;
      if (verbosity < 2)
        return G_LOG_WRITER_HANDLED;
      break;

    case G_LOG_LEVEL_DEBUG:
      if (any_domain && domain)
        break;
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

  for (guint i = 0; (!log_domain || !log_message) && i < n_fields; i++)
    {
      const GLogField *field = &fields[i];

      if (g_strcmp0 (field->key, "GLIB_DOMAIN") == 0)
        log_domain = field->value;
      else if (g_strcmp0 (field->key, "MESSAGE") == 0)
        log_message = field->value;
    }

  /* GdkPixbuf logs are too much verbose, skip unless asked not to. */
  if (g_strcmp0 (log_domain, "GdkPixbuf") == 0 &&
      g_strcmp0 (log_domain, domain) != 0)
    return G_LOG_WRITER_HANDLED;

  if (!log_domain)
    log_domain = "**";

  if (!log_message)
    log_message = "(NULL) message";

  if (any_domain)
    return chatty_log_write (log_level, log_domain, log_message,
                             fields, n_fields, user_data);

  if (!log_domain || strstr (log_domain, domain))
    return chatty_log_write (log_level, log_domain, log_message,
                             fields, n_fields, user_data);

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
