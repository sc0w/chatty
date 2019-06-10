/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "chatty-history.h"
#include <sqlite3.h>
#include <glib.h>
#include "stdio.h"
#include <sys/stat.h>

static sqlite3 *db;

static void
chatty_history_create_chat_schema(void)
{
  int rc;
  char *sql;
  char *zErrMsg = 0;

  sql = "CREATE TABLE IF NOT EXISTS chatty_chat("  \
    "id                 INTEGER     NOT NULL    PRIMARY KEY AUTOINCREMENT," \
    "timestamp          INTEGER     NOT_NULL    UNIQUE,"  \
    "direction          INTEGER     NOT NULL," \
    "conv_name          TEXT,"  \
    "jid                TEXT        NOT_NULL," \
    "alias              TEXT,"  \
    "uid                TEXT        NOT_NULL," \
    "message            TEXT" \
    ");";
  // TODO: LELAND: 'uid' to be implemented by XEP-0313. By now, using timestamp as UNIQUE to avoid dups in db
  // TODO: LELAND: Should 'alias' be NOT NULL?

  rc = sqlite3_exec(db, sql, NULL, NULL, &zErrMsg);

  if( rc != SQLITE_OK ){
    g_debug("SQL error when creating chatty_chat: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  } else {
    g_debug("chatty_chat table created successfully\n");
  }
}


static void
chatty_history_create_im_schema(void)
{
  int rc;
  char *sql;
  char *zErrMsg = 0;

  sql = "CREATE TABLE IF NOT EXISTS chatty_im("  \
    "id                 INTEGER     NOT NULL    PRIMARY KEY AUTOINCREMENT," \
    "timestamp          INTEGER     NOT_NULL    UNIQUE,"  \
    "direction          INTEGER     NOT NULL," \
    "account            TEXT        NOT_NULL," \
    "jid                TEXT        NOT_NULL," \
    "uid                TEXT        NOT_NULL," \
    "message            TEXT" \
    ");";
  // TODO: LELAND: 'uid' to be implemented by XEP-0313. By now, using timestamp as UNIQUE to avoid dups in db

  rc = sqlite3_exec(db, sql, NULL, NULL, &zErrMsg);

  if( rc != SQLITE_OK ){
    g_debug("SQL error when creating chatty_im: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  } else {
    g_debug("chatty_im table created successfully\n");
  }

}


static void
chatty_history_create_schemas(void)
{

  chatty_history_create_chat_schema();
  chatty_history_create_im_schema(); // TODO: LELAND: Define chat schema

}


int
chatty_history_open(void)
{
  //TODO: LELAND: What happens for multiple opens. Does SQL3 handle it?

  int rc;
  char *db_path;

  db_path =  g_build_filename (purple_user_dir(), "chatty", "db", NULL);
  g_mkdir_with_parents (db_path, S_IRWXU);
  db_path = g_build_filename(db_path, "chatty-history.db", NULL);
  rc = sqlite3_open(db_path, &db);
  g_free(db_path);

  chatty_history_create_schemas();

  if( rc ) {
    g_debug("Can't open database: %s\n", sqlite3_errmsg(db));
    return 0;
  } else {
    g_debug("Opened database successfully\n");
    return 1;
  }

}


void
chatty_history_close(void)
{
  // TODO: LELAND: Multiple closes?
  int close;

  close = sqlite3_close(db);

  switch (close)
    {
    case SQLITE_OK:
      g_debug("Database closed successfully\n");
      break;
    default:
      g_debug("Database could not be closed\n");
      break;
    }

}


void
chatty_history_add_chat_message(int         conv_id,
                            const char *message,
                            int         direction,
                            const char *jid,
                            const char *alias,
                            const char *uid,
                            time_t      mtime,
                            const char *conv_name)
{

  sqlite3_stmt *stmt;
  int Err;

  // TODO: LELAND: Watch out sqli!
  // TODO: LELAND: Once schemas are fixed, make col indexes #defines

  Err = sqlite3_prepare_v2(db, "INSERT INTO chatty_chat VALUES (?, ?, ?, ?, ?, ?, ?, ?)", -1, &stmt, NULL);
  if (Err != SQLITE_OK)
      g_debug("Error preparing %d", Err);

  Err = sqlite3_bind_text(stmt, 8, message, -1, SQLITE_TRANSIENT);
  if (Err != SQLITE_OK)
      g_debug("Error binding");//TODO: LELAND: Handle errors
  Err = sqlite3_bind_int(stmt, 3, direction);
  if (Err != SQLITE_OK)
      g_debug("Error binding");//TODO: LELAND: Handle errors
  Err = sqlite3_bind_text(stmt, 5, jid, -1, SQLITE_TRANSIENT);
  if (Err != SQLITE_OK)
      g_debug("Error binding");//TODO: LELAND: Handle errors
  Err = sqlite3_bind_text(stmt, 7, uid, -1, SQLITE_TRANSIENT);
  if (Err != SQLITE_OK)
      g_debug("Error binding");//TODO: LELAND: Handle errors
  Err = sqlite3_bind_int64(stmt, 2, mtime); // TODO: LELAND: Do we need human readable format?
                                            // TODO: LELAND: Do we need a local tstamp?
  if (Err != SQLITE_OK)
      g_debug("Error binding");//TODO: LELAND: Handle errors
  Err = sqlite3_bind_text(stmt, 4, conv_name, -1, SQLITE_TRANSIENT);
  if (Err != SQLITE_OK)
      g_debug("Error binding");//TODO: LELAND: Handle errors
  Err = sqlite3_bind_text(stmt, 6, alias, -1, SQLITE_TRANSIENT);
  if (Err != SQLITE_OK)
      g_debug("Error binding");//TODO: LELAND: Handle errors
                               //
  Err = sqlite3_step(stmt);
  if (Err != SQLITE_DONE)//TODO: LELAND: Handle errors
  {
      // TODO: LELAND: Handle
      g_debug("Error step %d, %s", Err, sqlite3_errmsg(db));
  }

  Err = sqlite3_finalize(stmt);//TODO: LELAND: Handle errors

}


void
chatty_history_add_im_message(
                            const char *message,
                            int         direction,
                            const char *account,
                            const char *jid,
                            const char *uid,
                            time_t      mtime
                           )
{

  sqlite3_stmt *stmt;
  int Err;

  // TODO: LELAND: Watch out sqli!

  Err = sqlite3_prepare_v2(db, "INSERT INTO chatty_im VALUES (?, ?, ?, ?, ?, ?, ?)", -1, &stmt, NULL);
  if (Err != SQLITE_OK)
      g_debug("Error preparing %d", Err);

  Err = sqlite3_bind_text(stmt, 7, message, -1, SQLITE_TRANSIENT);
  if (Err != SQLITE_OK)
      g_debug("Error binding");//TODO: LELAND: Handle errors

  Err = sqlite3_bind_int(stmt, 3, direction);
  if (Err != SQLITE_OK)
      g_debug("Error binding");//TODO: LELAND: Handle errors

  Err = sqlite3_bind_text(stmt, 5, jid, -1, SQLITE_TRANSIENT);
  if (Err != SQLITE_OK)
      g_debug("Error binding");//TODO: LELAND: Handle errors

  Err = sqlite3_bind_text(stmt, 4, account, -1, SQLITE_TRANSIENT);
  if (Err != SQLITE_OK)
      g_debug("Error binding");//TODO: LELAND: Handle errors

  Err = sqlite3_bind_text(stmt, 6, uid, -1, SQLITE_TRANSIENT);
  if (Err != SQLITE_OK)
      g_debug("Error binding");//TODO: LELAND: Handle errors

  Err = sqlite3_bind_int64(stmt, 2, mtime); // TODO: LELAND: Do we need human readable format?
                                            // TODO: LELAND: Do we need a local tstamp?
  if (Err != SQLITE_OK)
      g_debug("Error binding");//TODO: LELAND: Handle errors


  Err = sqlite3_step(stmt);
  if (Err != SQLITE_DONE)//TODO: LELAND: Handle errors
  {
      // TODO: LELAND: Handle
      g_debug("Error step %d, %s", Err, sqlite3_errmsg(db));
  }

  Err = sqlite3_finalize(stmt);//TODO: LELAND: Handle errors

}

int
chatty_history_get_chat_last_message_time(const char* conv_name)
{

  int Err;
  sqlite3_stmt *stmt;
  int time_stamp  = 0;

  // TODO: LELAND: Watch out sqli!
  g_debug ("@LELAND@ chatty_history_get_chat_last_message_time for conv_name %s", conv_name);

  Err = sqlite3_prepare_v2(db, "SELECT max(timestamp) FROM chatty_chat WHERE conv_name=(?)", -1, &stmt, NULL);
  if (Err != SQLITE_OK)
      g_debug("Error preparing %d", Err); //TODO: LELAND: Handle errors

  Err = sqlite3_bind_text(stmt, 1, conv_name, -1, SQLITE_TRANSIENT);
  if (Err != SQLITE_OK)
      g_debug("Error binding");//TODO: LELAND: Handle errors

  g_debug ("@LELAND@ Chat Last Time");
  while ((sqlite3_step(stmt)) == SQLITE_ROW) {
      time_stamp = sqlite3_column_int(stmt, 0);
      g_debug ("@LELAND@ Chat Last Time: Found row! %d", time_stamp);
  }

  Err = sqlite3_finalize(stmt);//TODO: LELAND: Handle errors
  if (Err != SQLITE_OK)
      g_debug("Error finalizing");//TODO: LELAND: Handle errors

  return time_stamp;
}

void
chatty_history_get_chat_messages(const char* conv_name,
                               void (*callback)(char* msg, int direction, int time_stamp, char *from, char *alias, ChattyConversation *chatty_conv),
                               ChattyConversation *chatty_conv)
{

  int Err;
  sqlite3_stmt *stmt;
  char *msg;
  int time_stamp;
  int direction;
  char *from;
  char *alias;

  // TODO: LELAND: Watch out sqli!

  Err = sqlite3_prepare_v2(db, "SELECT * FROM chatty_chat WHERE conv_name=(?)", -1, &stmt, NULL);
  if (Err != SQLITE_OK)
      g_debug("Error preparing %d", Err); //TODO: LELAND: Handle errors

  Err = sqlite3_bind_text(stmt, 1, conv_name, -1, SQLITE_TRANSIENT);
  if (Err != SQLITE_OK)
      g_debug("Error binding");//TODO: LELAND: Handle errors

  while ((sqlite3_step(stmt)) == SQLITE_ROW) {
      time_stamp = sqlite3_column_int(stmt, 1);
      direction = sqlite3_column_int(stmt, 2);
      msg = sqlite3_column_text(stmt, 7);
      from = sqlite3_column_text(stmt, 4);
      alias = sqlite3_column_text(stmt, 5);
      callback(msg, time_stamp, direction, from, alias, chatty_conv);
  }

  Err = sqlite3_finalize(stmt);//TODO: LELAND: Handle errors
  if (Err != SQLITE_OK)
      g_debug("Error finalizing");//TODO: LELAND: Handle errors

}


void
chatty_history_get_im_messages(const char* account,
                               const char* jid,
                               void (*callback)(char* msg, int direction, int time_stamp, ChattyConversation *chatty_conv),
                               ChattyConversation *chatty_conv)
{

  int Err;
  sqlite3_stmt *stmt;
  char *msg;
  int time_stamp;
  int direction;

  // TODO: LELAND: Watch out sqli!

  Err = sqlite3_prepare_v2(db, "SELECT * FROM chatty_im WHERE account=(?) AND jid=(?)", -1, &stmt, NULL);
  if (Err != SQLITE_OK)
      g_debug("Error preparing %d", Err);

  Err = sqlite3_bind_text(stmt, 1, account, -1, SQLITE_TRANSIENT);
  if (Err != SQLITE_OK)
      g_debug("Error binding");//TODO: LELAND: Handle errors

  Err = sqlite3_bind_text(stmt, 2, jid, -1, SQLITE_TRANSIENT);
  if (Err != SQLITE_OK)
      g_debug("Error binding");//TODO: LELAND: Handle errors

  while ((sqlite3_step(stmt)) == SQLITE_ROW) {
      time_stamp = sqlite3_column_int(stmt, 1);
      direction = sqlite3_column_int(stmt, 2);
      msg = sqlite3_column_text(stmt, 6);
      callback(msg, time_stamp, direction, chatty_conv);
  }

  Err = sqlite3_finalize(stmt);//TODO: LELAND: Handle errors
  if (Err != SQLITE_OK)
      g_debug("Error finalizing");//TODO: LELAND: Handle errors


}


void
chatty_history_remove_message(void)
{

}

void
chatty_history_delete_chat(const char* conv_name){
  int Err;
  sqlite3_stmt *stmt;

  // TODO: LELAND: Watch out sqli!
  g_debug ("@LELAND@ chatty_history_delete_chat for conv_name %s", conv_name);

  Err = sqlite3_prepare_v2(db, "DELETE FROM chatty_chat WHERE conv_name=(?)", -1, &stmt, NULL);
  if (Err != SQLITE_OK)
      g_debug("Error when removing chat from DDBB : %d", Err); //TODO: LELAND: Handle errors

  Err = sqlite3_bind_text(stmt, 1, conv_name, -1, SQLITE_TRANSIENT);
  if (Err != SQLITE_OK)
      g_debug("Error when removing chat from DDBB : %d", Err);//TODO: LELAND: Handle errors

  sqlite3_step(stmt);

  Err = sqlite3_finalize(stmt);//TODO: LELAND: Handle errors
  if (Err != SQLITE_OK)
      g_debug("Error when removing chat from DDBB : %d", Err);//TODO: LELAND: Handle errors

}

                                   
