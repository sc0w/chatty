/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include <glib.h>
#include <glib/gi18n.h>
#include "chatty-utils.h"
#include <libebook-contacts/libebook-contacts.h>


#define SECONDS_PER_MINUTE 60.0
#define SECONDS_PER_HOUR   3600.0
#define SECONDS_PER_DAY    86400.0
#define SECONDS_PER_MONTH  2592000.0
#define SECONDS_PER_YEAR   31536000.0


char *
chatty_utils_time_ago_in_words (time_t             time_stamp,
                                ChattyTimeAgoFlags flags)
{
  // based on the ruby on rails method 'distance_of_time_in_words'

  time_t time_now;
  struct tm  *timeinfo;

  g_autofree gchar *iso_timestamp = NULL;

  const char *unit;
  const char *prefix;

  const char *str_about, *str_less_than;
  const char *str_seconds, *str_minute, *str_minutes;
  const char *str_hour, *str_hours, *str_day, *str_days;
  const char *str_month, *str_months, *str_year, *str_years;

  int number, seconds, minutes, hours, days, months, years, offset, remainder;

  gboolean show_date = FALSE;

  double dist_in_seconds;

  if (flags & CHATTY_UTILS_TIME_AGO_VERBOSE) {
    str_about     = _("About ");
    str_less_than = _("Less than ");
    str_seconds   = _(" seconds");
    str_minute    = _(" minute");
    str_minutes   = _(" minutes");
    str_hour      = _(" hour");
    str_hours     = _(" hours");
    str_day       = _(" day");
    str_days      = _(" days");
    str_month     = _(" month");
    str_months    = _(" months");
    str_year      = _(" year");
    str_years     = _(" years");
  } else {
    str_about     = "~";
    str_less_than = (flags & CHATTY_UTILS_TIME_AGO_NO_MARKUP) ? "<" : "&lt;";
    str_seconds   = _("s");
    str_minute    = _("m");
    str_minutes   = _("m");
    str_hour      = _("h");
    str_hours     = _("h");
    str_day       = _("d");
    str_days      = _("d");
    str_month     = _("mo");
    str_months    = _("mos");
    str_year      = _("y");
    str_years     = _("y");
  }

  time (&time_now);

  timeinfo = localtime (&time_stamp);

  iso_timestamp = g_malloc0 (MAX_GMT_ISO_SIZE * sizeof(char));

  strftime (iso_timestamp,
            MAX_GMT_ISO_SIZE * sizeof(char),
            "%d.%m.%y",
            timeinfo);

  dist_in_seconds = difftime (time_now, time_stamp);

  seconds = (int)dist_in_seconds;
  minutes = (int)(dist_in_seconds / SECONDS_PER_MINUTE);
  hours   = (int)(dist_in_seconds / SECONDS_PER_HOUR);
  days    = (int)(dist_in_seconds / SECONDS_PER_DAY);
  months  = (int)(dist_in_seconds / SECONDS_PER_MONTH);
  years   = (int)(dist_in_seconds / SECONDS_PER_YEAR);

  switch (minutes) {
    case 0 ... 1:
      unit = str_seconds;

      switch (seconds) {
        case 0 ... 14:
          prefix = str_less_than;
          number = 15;
          break;
        case 15 ... 29:
          prefix = str_less_than;
          number = 30;
          break;
        case 30 ... 59:
          prefix = str_less_than;
          number = 1;
          unit = str_minute;
          break;
        default:
          prefix = str_about;
          number = 1;
          unit = str_minute;
          break;
      }
      break;

    case 2 ... 44:
      prefix = "";
      number = minutes;
      unit = str_minutes;
      break;
    case 45 ... 89:
      prefix = str_about;
      number = 1;
      unit = str_hour;
      break;
    case 90 ... 1439:
      prefix = str_about;
      number = hours;
      unit = str_hours;
      break;
    case 1440 ... 2529:
      prefix = str_about;
      number = 1;
      unit = str_day;
      show_date = flags & CHATTY_UTILS_TIME_AGO_SHOW_DATE;
      break;
    case 2530 ... 43199:
      prefix = "";
      number = days;
      unit = str_days;
      show_date = flags & CHATTY_UTILS_TIME_AGO_SHOW_DATE;
      break;
    case 43200 ... 86399:
      prefix = str_about;
      number = 1;
      unit = str_month;
      show_date = flags & CHATTY_UTILS_TIME_AGO_SHOW_DATE;
      break;
    case 86400 ... 525600:
      prefix = "";
      number = months;
      unit = str_months;
      show_date = flags & CHATTY_UTILS_TIME_AGO_SHOW_DATE;
      break;

    default:
      number = years;

      unit = (number == 1) ? str_year : str_years;

      offset = (int)((float)years / 4.0) * 1440.0;

      remainder = (minutes - offset) % 525600;

      show_date = flags & CHATTY_UTILS_TIME_AGO_SHOW_DATE;

      if (remainder < 131400) {
        prefix = str_about;
      } else if (remainder < 394200) {
        prefix = _("Over");
      } else {
        ++number;
        unit = str_years;
        prefix = _("Almost");
      }
      break;
  }

  return show_date ? g_strdup_printf ("%s", iso_timestamp) :
                     g_strdup_printf ("%s%d%s", prefix, number, unit);
}


char *
chatty_utils_strip_blanks (const char *string)
{
  char *result;
  char **chunks;

  chunks = g_strsplit (string, "%20", 0);

  result = g_strjoinv(NULL, chunks);
  
  g_strstrip (result);

  g_strfreev (chunks);

  return result;
}


char *
chatty_utils_strip_cr_lf (const char *string)
{
  char *result;
  char **chunks;

  chunks = g_strsplit_set (string, "\r\n", 0);

  result = g_strjoinv(" ", chunks);
  
  g_strstrip (result);

  g_strfreev (chunks);

  return result;
}


char* 
chatty_utils_check_phonenumber (const char *phone_number)
{
  EPhoneNumber      *number;
  g_autofree char   *stripped = NULL;
  char              *result;
  g_autoptr(GError)  err = NULL;

  stripped = chatty_utils_strip_blanks (phone_number);

  number = e_phone_number_from_string (stripped, NULL, &err);

  if (!number || !e_phone_number_is_supported ()) {
    g_debug ("%s %s: %s\n", __func__, phone_number, err->message);

    result = NULL;
  } else {
    if (g_strrstr (phone_number, "+")) {
      result = e_phone_number_to_string (number, E_PHONE_NUMBER_FORMAT_E164);
    } else {
      result = e_phone_number_get_national_number (number);
    }
  }

  e_phone_number_free (number);

  return result;
}


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

gboolean
chatty_utils_get_item_position (GListModel *list,
                                gpointer    item,
                                guint      *position)
{
  guint n_items;

  g_return_val_if_fail (G_IS_LIST_MODEL (list), FALSE);
  g_return_val_if_fail (item != NULL, FALSE);

  n_items = g_list_model_get_n_items (list);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GObject) object = NULL;

      object = g_list_model_get_item (list, i);

      if (object == item)
        {
          if (position)
            *position = i;

          return TRUE;
        }
    }

  return FALSE;
}

/* XXX: a helper API till the dust settles */
ChattyPpAccount *
chatty_pp_account_find (PurpleAccount *account)
{
  chatty_data_t *chatty;
  PurpleAccount *pp_account;
  GListModel *model;
  guint n_items;

  g_return_val_if_fail (account, NULL);

  chatty = chatty_get_data ();
  model = G_LIST_MODEL (chatty->account_list);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(ChattyPpAccount) object = NULL;

      object = g_list_model_get_item (model, i);
      pp_account = chatty_pp_account_get_account (object);

      if (pp_account == account)
        return object;
    }

  return NULL;
}

/* XXX: a helper API till the dust settles */
gboolean
chatty_pp_account_remove (ChattyPpAccount *self)
{
  chatty_data_t *chatty;
  GListModel *model;
  guint n_items;

  g_return_val_if_fail (CHATTY_IS_PP_ACCOUNT (self), FALSE);

  chatty = chatty_get_data ();
  model = G_LIST_MODEL (chatty->account_list);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(ChattyPpAccount) object = NULL;

      object = g_list_model_get_item (model, i);

      if (object == self)
        {
          g_list_store_remove (chatty->account_list, i);

          return TRUE;
        }
    }

  return FALSE;
}
