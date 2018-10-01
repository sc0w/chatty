/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef __MSG_LIST_H_INCLUDE__
#define __MSG_LIST_H_INCLUDE__

#include <gtk/gtk.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define CHATTY_TYPE_MSG_LIST (chatty_msg_list_get_type())

G_DECLARE_DERIVABLE_TYPE (ChattyMsgList, chatty_msg_list, CHATTY, MSG_LIST, GtkBox)


struct _ChattyMsgListClass
{
  GtkBoxClass parent_class;
};


typedef struct {
  const char *str_0;
  const char *str_1;
  const char *str_2;
} header_strings_t;


enum {
  MSG_LIST_HEADER     = 1 << 0,
  MSG_LIST_RULER      = 1 << 1,
  MSG_LIST_TIMESTAMP  = 1 << 2,
  ALL_FEATURES        = 0x7
} message_list_features;


enum {
  MSG_TYPE_IM,
  MSG_TYPE_IM_E2EE,
  MSG_TYPE_SMS,
  MSG_TYPE_LAST
} e_msg_type;


enum {
  MSG_IS_OUTGOING,
  MSG_IS_INCOMING
} e_msg_dir;


GtkWidget *chatty_msg_list_new (guint message_type,
                                gboolean disclaimer);

void chatty_msg_list_add_message (ChattyMsgList *self,
                                  guint message_dir,
                                  const gchar *message,
                                  const gchar *footer);

void chatty_msg_list_clear (ChattyMsgList *self);
void chatty_msg_list_autoscroll (ChattyMsgList *self);

void chatty_msg_list_show_typing_indicator (ChattyMsgList *self);
void chatty_msg_list_hide_typing_indicator (ChattyMsgList *self);

guint chatty_msg_list_get_msg_type (ChattyMsgList *self);
void chatty_msg_list_set_msg_type (ChattyMsgList *self,
                                   guint         message_type);

G_END_DECLS

#endif
