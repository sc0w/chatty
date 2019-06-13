/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-history"

#define CHAT_ID_IDX         1
#define CHAT_TIMESTAMP_IDX  2
#define CHAT_DIRECTION_IDX  3
#define CHAT_FROM_IDX       4
#define CHAT_TO_IDX         5
#define CHAT_ALIAS_IDX      6
#define CHAT_UID_IDX        7
#define CHAT_MESSAGE_IDX    8

#define IM_ID_IDX         1
#define IM_TIMESTAMP_IDX  2
#define IM_DIRECTION_IDX  3
#define IM_FROM_IDX       4
#define IM_TO_IDX         5
#define IM_UID_IDX        6
#define IM_MESSAGE_IDX    7

#include "chatty-history.h"
#include <sqlite3.h>
#include <glib.h>
#include "stdio.h"
#include <sys/stat.h>

static sqlite3 *db;

static void
chatty_history_create_chat_schema (void)
{
  int rc;
  char *sql;
  char *zErrMsg = 0;

  sql = "CREATE TABLE IF NOT EXISTS chatty_chat("  \
    "id                 INTEGER     NOT NULL    PRIMARY KEY AUTOINCREMENT," \
    "timestamp          INTEGER     NOT_NULL," \
    "direction          INTEGER     NOT NULL," \
    "from_id            TEXT        NOT NULL," \
    "to_id              TEXT        NOT_NULL," \
    "alias              TEXT,"  \
    "uid                TEXT        NOT_NULL," \
    "message            TEXT," \
    "UNIQUE (timestamp, message)"
    ");";
  // TODO: LELAND: 'uid' to be implemented by XEP-0313. By now, using UNIQUE constraint to avoid dups in db

  rc = sqlite3_exec(db, sql, NULL, NULL, &zErrMsg);

  if( rc != SQLITE_OK ){
    g_debug("Error when creating chatty_chat table. errno: %d, desc: %s. %s", rc, sqlite3_errmsg(db), zErrMsg);
    sqlite3_free(zErrMsg);
  } else {
    g_debug("chatty_chat table created successfully");
  }

}


static void
chatty_history_create_im_schema (void)
{
  int rc;
  char *sql;
  char *zErrMsg = 0;

  sql = "CREATE TABLE IF NOT EXISTS chatty_im("  \
    "id                 INTEGER     NOT NULL    PRIMARY KEY AUTOINCREMENT," \
    "timestamp          INTEGER     NOT_NULL,"  \
    "direction          INTEGER     NOT NULL," \
    "from_id            TEXT        NOT_NULL," \
    "to_id              TEXT        NOT_NULL," \
    "uid                TEXT        NOT_NULL," \
    "message            TEXT," \
    "UNIQUE (timestamp, message)"
    ");";
  // TODO: LELAND: 'uid' to be implemented by XEP-0313. By now, using UNIQUE constraint to avoid dups in db

  rc = sqlite3_exec(db, sql, NULL, NULL, &zErrMsg);

  if( rc != SQLITE_OK ){
    g_debug("Error when creating chatty_im table. errno: %d, desc: %s. %s", rc, sqlite3_errmsg(db), zErrMsg);
    sqlite3_free(zErrMsg);
  } else {
    g_debug("chatty_im table created successfully");
  }

}


static void
chatty_history_create_schemas (void)
{
  chatty_history_create_chat_schema();
  chatty_history_create_im_schema();
}


int
chatty_history_open (void)
{
  int rc;
  char *db_path;

  if(db == NULL){
    db_path =  g_build_filename (purple_user_dir(), "chatty", "db", NULL);
    g_mkdir_with_parents (db_path, S_IRWXU);
    db_path = g_build_filename(db_path, "chatty-history.db", NULL);
    rc = sqlite3_open(db_path, &db);
    g_free(db_path);

    if (rc != SQLITE_OK){
      g_debug("Database could not be opened. errno: %d, desc: %s", rc, sqlite3_errmsg(db));
    } else {
      g_debug("Database opened successfully");
    }

    chatty_history_create_schemas();
  }

  return 1;

}


void
chatty_history_close (void)
{
  int rc;

  if(db != NULL){
    rc = sqlite3_close(db);

    if (rc != SQLITE_OK){
      g_debug("Database could not be closed. errno: %d, desc: %s", rc, sqlite3_errmsg(db));
    } else {
      g_debug("Database closed successfully");
    }
  }

}


void
chatty_history_add_chat_message ( const char *message,
                                  int         direction,
                                  const char *from,
                                  const char *alias,
                                  const char *uid,
                                  time_t      mtime,
                                  const char *to)
{

  sqlite3_stmt *stmt;
  int rc;

  rc = sqlite3_prepare_v2(db, "INSERT INTO chatty_chat VALUES (?, ?, ?, ?, ?, ?, ?, ?)", -1, &stmt, NULL);
  if (rc != SQLITE_OK)
      g_debug("Error preparing statement when adding CHAT message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, CHAT_MESSAGE_IDX, message, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding CHAT message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_int(stmt, CHAT_DIRECTION_IDX, direction);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding CHAT message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, CHAT_FROM_IDX, from, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding CHAT message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, CHAT_UID_IDX, uid, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding CHAT message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_int64(stmt, CHAT_TIMESTAMP_IDX, mtime);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding CHAT message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, CHAT_TO_IDX, to, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding CHAT message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, CHAT_ALIAS_IDX, alias, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding CHAT message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE)
      g_debug("Error in step when adding CHAT message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_finalize(stmt);
  if (rc != SQLITE_OK)
    g_debug("Error finalizing when adding CHAT message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

}


void
chatty_history_add_im_message (const char *message,
                              int         direction,
                              const char *from,
                              const char *to,
                              const char *uid,
                              time_t      mtime)
{

  sqlite3_stmt *stmt;
  int rc;

  rc = sqlite3_prepare_v2(db, "INSERT INTO chatty_im VALUES (?, ?, ?, ?, ?, ?, ?)", -1, &stmt, NULL);
  if (rc != SQLITE_OK)
      g_debug("Error preparing statement when adding IM message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, IM_MESSAGE_IDX, message, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding IM message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_int(stmt, IM_DIRECTION_IDX, direction);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding IM message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, IM_TO_IDX, to, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding IM message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, IM_FROM_IDX, from, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding IM message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, IM_UID_IDX, uid, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding IM message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_int64(stmt, IM_TIMESTAMP_IDX, mtime);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding IM message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE)
      g_debug("Error in step when adding IM message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_finalize(stmt);
  if (rc != SQLITE_OK)
      g_debug("Error finalizing when adding IM message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

}


// TODO: LELAND: BUG! 'from' is needed
int
chatty_history_get_chat_last_message_time (const char* to)
{

  int rc;
  sqlite3_stmt *stmt;
  int time_stamp  = 0;

  rc = sqlite3_prepare_v2(db, "SELECT max(timestamp) FROM chatty_chat WHERE to_id=(?)", -1, &stmt, NULL);
  if (rc != SQLITE_OK)
      g_debug("Error preparing when getting chat last message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 1, to, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding when getting chat last message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  while ((sqlite3_step(stmt)) == SQLITE_ROW) {
      time_stamp = sqlite3_column_int(stmt, 0);
  }

  rc = sqlite3_finalize(stmt);
  if (rc != SQLITE_OK)
      g_debug("Error finalizing when getting chat last message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  return time_stamp;
}


// TODO: LELAND: BUG! 'from' is needed
void
chatty_history_get_chat_messages (const char* to,
                                  void (*cb)( const unsigned char* msg,
                                              int direction,
                                              int time_stamp,
                                              const unsigned char *from,
                                              const unsigned char *alias,
                                              ChattyConversation *chatty_conv),
                                  ChattyConversation *chatty_conv)
{

  int rc;
  sqlite3_stmt *stmt;
  const unsigned char *msg;
  int time_stamp;
  int direction;
  const unsigned char *from;
  const unsigned char *alias;

  rc = sqlite3_prepare_v2(db, "SELECT * FROM chatty_chat WHERE to_id=(?) ORDER BY timestamp ASC", -1, &stmt, NULL);
  if (rc != SQLITE_OK)
      g_debug("Error preparing statement when querying CHAT messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 1, to, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding values when querying CHAT messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  while ((sqlite3_step(stmt)) == SQLITE_ROW) {
      time_stamp = sqlite3_column_int(stmt, 1);
      direction = sqlite3_column_int(stmt, 2);
      msg = sqlite3_column_text(stmt, 7);
      from = sqlite3_column_text(stmt, 4);
      alias = sqlite3_column_text(stmt, 5);
      cb(msg, time_stamp, direction, from, alias, chatty_conv);
  }

  rc = sqlite3_finalize(stmt);
  if (rc != SQLITE_OK)
    g_debug("Error finalizing when querying CHAT messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

}


void
chatty_history_get_im_messages (const char* from,
                                const char* to,
                                void (*cb)(const unsigned char* msg,
                                           int direction,
                                           int time_stamp,
                                           ChattyConversation *chatty_conv),
                                ChattyConversation *chatty_conv)
{

  int rc;
  sqlite3_stmt *stmt;
  const unsigned char *msg;
  int time_stamp;
  int direction;

  rc = sqlite3_prepare_v2(db, "SELECT * FROM chatty_im WHERE from_id=(?) AND to_id=(?) ORDER BY timestamp ASC", -1, &stmt, NULL);
  if (rc != SQLITE_OK)
      g_debug("Error preparing statement when querying IM messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 1, from, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding when querying IM messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 2, to, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
    g_debug("Error binding when querying IM messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  while ((sqlite3_step(stmt)) == SQLITE_ROW) {
      time_stamp = sqlite3_column_int(stmt, 1);
      direction = sqlite3_column_int(stmt, 2);
      msg = sqlite3_column_text(stmt, 6);
      cb(msg, time_stamp, direction, chatty_conv);
  }

  rc = sqlite3_finalize(stmt);
  if (rc != SQLITE_OK)
      g_debug("Error finalizing when querying IM messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

}


void
chatty_history_delete_chat (const char* from,
                            const char* to){
  int rc;
  sqlite3_stmt *stmt;

  rc = sqlite3_prepare_v2(db, "DELETE FROM chatty_chat WHERE from_id=(?) AND to_id=(?)", -1, &stmt, NULL);
  if (rc != SQLITE_OK)
      g_debug("Error preparing statement when deleting CHAT messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 1, from, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding when deleting CHAT messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 2, to, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding when deleting CHAT messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  sqlite3_step(stmt);
  if (rc != SQLITE_OK)
      g_debug("Error in step when deleting CHAT messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_finalize(stmt);
  if (rc != SQLITE_OK)
      g_debug("Error finalizing when deleting CHAT messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

}


void
chatty_history_delete_im (const char *from,
                          const char *to){
  int rc;
  sqlite3_stmt *stmt;

  rc = sqlite3_prepare_v2(db, "DELETE FROM chatty_im WHERE from_id=(?) AND to_id=(?)", -1, &stmt, NULL);
  if (rc != SQLITE_OK)
      g_debug("Error preparing statement when deleting IM messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 1, from, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding when deleting IM messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 2, to, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding when deleting IM messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));
                                                            //
  sqlite3_step(stmt);
  if (rc != SQLITE_OK)
      g_debug("Error in step when deleting IM messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_finalize(stmt);
  if (rc != SQLITE_OK)
      g_debug("Error finalizing when deleting IM messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

}
                                   
