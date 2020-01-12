/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* purple-init.h
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>
#include <purple.h>

G_BEGIN_DECLS

void test_purple_init (void);

static guint
glib_input_add (gint                 fd,
                PurpleInputCondition condition,
                PurpleInputFunction  function,
                gpointer             data)
{
  return 0;
}

static
PurpleEventLoopUiOps eventloop_ops =
{
  g_timeout_add,
  g_source_remove,
  glib_input_add,
  g_source_remove,
  NULL,
  g_timeout_add_seconds,
};

void
test_purple_init (void)
{
  const char *build_dir;
  g_autofree char *path = NULL;

  build_dir = g_getenv ("G_TEST_BUILDDIR");
  g_warn_if_fail (build_dir);
  path = g_build_path (G_DIR_SEPARATOR_S, build_dir, ".purple", NULL);
  purple_util_set_user_dir (path);

  purple_eventloop_set_ui_ops (&eventloop_ops);

  g_assert_true (purple_core_init ("chatty-test"));
  g_assert_true (purple_core_ensure_single_instance ());

  purple_set_blist (purple_blist_new ());
}

G_END_DECLS
