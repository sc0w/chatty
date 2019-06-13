/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-history"

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
    "timestamp          INTEGER     NOT_NULL,"  \
    "direction          INTEGER     NOT NULL," \
    "conv_name          TEXT,"  \
    "jid                TEXT        NOT_NULL," \
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
    "account            TEXT        NOT_NULL," \
    "jid                TEXT        NOT_NULL," \
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
                                  const char *jid,
                                  const char *alias,
                                  const char *uid,
                                  time_t      mtime,
                                  const char *conv_name)
{

  sqlite3_stmt *stmt;
  int rc;

  // TODO: LELAND: Once schemas are fixed, make col indexes #defines

  rc = sqlite3_prepare_v2(db, "INSERT INTO chatty_chat VALUES (?, ?, ?, ?, ?, ?, ?, ?)", -1, &stmt, NULL);
  if (rc != SQLITE_OK)
      g_debug("Error preparing statement when adding CHAT message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 8, message, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding CHAT message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_int(stmt, 3, direction);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding CHAT message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 5, jid, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding CHAT message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 7, uid, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding CHAT message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_int64(stmt, 2, mtime);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding CHAT message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 4, conv_name, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding CHAT message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 6, alias, -1, SQLITE_TRANSIENT);
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
                              const char *account,
                              const char *jid,
                              const char *uid,
                              time_t      mtime)
{

  sqlite3_stmt *stmt;
  int rc;

  rc = sqlite3_prepare_v2(db, "INSERT INTO chatty_im VALUES (?, ?, ?, ?, ?, ?, ?)", -1, &stmt, NULL);
  if (rc != SQLITE_OK)
      g_debug("Error preparing statement when adding IM message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 7, message, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding IM message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_int(stmt, 3, direction);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding IM message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 5, jid, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding IM message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 4, account, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding IM message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 6, uid, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding IM message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_int64(stmt, 2, mtime);
  if (rc != SQLITE_OK)
      g_debug("Error binding value when adding IM message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE)
      g_debug("Error in step when adding IM message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_finalize(stmt);
  if (rc != SQLITE_OK)
      g_debug("Error finalizing when adding IM message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

}


int
chatty_history_get_chat_last_message_time (const char* conv_name)
{

  int rc;
  sqlite3_stmt *stmt;
  int time_stamp  = 0;

  rc = sqlite3_prepare_v2(db, "SELECT max(timestamp) FROM chatty_chat WHERE conv_name=(?)", -1, &stmt, NULL);
  if (rc != SQLITE_OK)
      g_debug("Error preparing when getting chat last message. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 1, conv_name, -1, SQLITE_TRANSIENT);
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


void
chatty_history_get_chat_messages (const char* conv_name,
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

  rc = sqlite3_prepare_v2(db, "SELECT * FROM chatty_chat WHERE conv_name=(?) ORDER BY timestamp ASC", -1, &stmt, NULL);
  if (rc != SQLITE_OK)
      g_debug("Error preparing statement when querying CHAT messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 1, conv_name, -1, SQLITE_TRANSIENT);
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
chatty_history_get_im_messages (const char* account,
                                const char* jid,
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

  rc = sqlite3_prepare_v2(db, "SELECT * FROM chatty_im WHERE account=(?) AND jid=(?) ORDER BY timestamp ASC", -1, &stmt, NULL);
  if (rc != SQLITE_OK)
      g_debug("Error preparing statement when querying IM messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 1, account, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding when querying IM messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 2, jid, -1, SQLITE_TRANSIENT);
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


// TODO: LELAND: BUG!: this method has to take also the account of the local user
// This implies to add it to the DDBB schema too!
void
chatty_history_delete_chat (const char* conv_name){
  int rc;
  sqlite3_stmt *stmt;

  rc = sqlite3_prepare_v2(db, "DELETE FROM chatty_chat WHERE conv_name=(?)", -1, &stmt, NULL);
  if (rc != SQLITE_OK)
      g_debug("Error preparing statement when deleting CHAT messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 1, conv_name, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding when deleting CHAT messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  sqlite3_step(stmt);
  if (rc != SQLITE_DONE)
      g_debug("Error in step when deleting CHAT messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_finalize(stmt);
  if (rc != SQLITE_OK)
      g_debug("Error finalizing when deleting CHAT messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

}


void
chatty_history_delete_im (const char *account,
                          const char *jid){
  int rc;
  sqlite3_stmt *stmt;

  g_debug ("@LELAND@ chatty_history_delete_im for account %s and buddy jid %s", account, jid);

  rc = sqlite3_prepare_v2(db, "DELETE FROM chatty_im WHERE account=(?) AND jid=(?)", -1, &stmt, NULL);
  if (rc != SQLITE_OK)
      g_debug("Error preparing statement when deleting IM messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 1, account, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding when deleting IM messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_bind_text(stmt, 2, jid, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      g_debug("Error binding when deleting IM messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));
                                                            //
  sqlite3_step(stmt);
  if (rc != SQLITE_DONE)
      g_debug("Error in step when deleting IM messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

  rc = sqlite3_finalize(stmt);
  if (rc != SQLITE_OK)
      g_debug("Error finalizing when deleting IM messages. errno: %d, desc: %s", rc, sqlite3_errmsg(db));

}
                                   
