/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "chatty-history.h"
#include <sqlite3.h>
#include <glib.h>

static sqlite3 *db;

static void
chatty_history_create_schema(void){
  int rc;
  char *sql;
  char *zErrMsg = 0;

  g_debug("@LELAND@ chatty_history_create_schema"); // TODO: LELAND: Remove

  sql = "CREATE TABLE IF NOT EXISTS history("  \
    "id                 INTEGER     NOT NULL    PRIMARY KEY AUTOINCREMENT," \
    "timestamp          INTEGER     NOT_NULL    UNIQUE,"  \
    "direction          INTEGER     NOT NULL," \
    "conv_id            INTEGER     NOT NULL," \
    "jid                TEXT        NOT_NULL," \
    "uid                TEXT        NOT_NULL," \
    "message            TEXT" \
    ");";
  // TODO: LELAND: 'uid' to be implemented by XEP-0313. By now, using timestamp as UNIQUE to avoid dups in db

  rc = sqlite3_exec(db, sql, NULL, NULL, &zErrMsg);

  if( rc != SQLITE_OK ){
    g_debug("SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  } else {
    g_debug("Table created successfully\n");
  }

}


int
chatty_history_open(void){
  int rc;

  //TODO: LELAND: What happens for multiple opens. Does SQL3 handle it?
  rc = sqlite3_open("chatty-history.db", &db); //TODO: LELAND: This file, where should it be located? right now at host ~

  chatty_history_create_schema();

  if( rc ) {
    g_debug("Can't open database: %s\n", sqlite3_errmsg(db));
    return 0;
  } else {
    g_debug("Opened database successfully\n");
    return 1;
  }

}


void
chatty_history_close(void){
  // TODO: LELAND: Multiple closes?
  int close;

  close = sqlite3_close(db);

  switch (close)
    {
    case SQLITE_OK:
      g_debug("Database closed successfully\n");
      break;
    case SQLITE_BUSY:
      g_debug("Database could not be closed\n");
      break;
    }

}


void
chatty_history_add_message(int conv_id, const char* message, int direction, const char* jid, const char* uid, time_t mtime){

  sqlite3_stmt *stmt;
  int Err;

  g_debug("@LELAND@ chatty_history_add_messages"); // TODO: LELAND: Remove
  // TODO: LELAND: Watch out sqli!

  Err = sqlite3_prepare_v2(db, "INSERT INTO history VALUES (?, ?, ?, ?, ?, ?, ?)", -1, &stmt, NULL);
  if (Err != SQLITE_OK)
      g_debug("Error preparing %d", Err);

  Err = sqlite3_bind_int(stmt, 4, conv_id);
  if (Err != SQLITE_OK)
      g_debug("Error binding"); //TODO: LELAND: Handle errors
  Err = sqlite3_bind_text(stmt, 7, message, -1, SQLITE_TRANSIENT);
  if (Err != SQLITE_OK)
      g_debug("Error binding");//TODO: LELAND: Handle errors
  Err = sqlite3_bind_int(stmt, 3, direction);
  if (Err != SQLITE_OK)
      g_debug("Error binding");//TODO: LELAND: Handle errors
  Err = sqlite3_bind_text(stmt, 5, jid, -1, SQLITE_TRANSIENT);
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


void
chatty_history_get_messages(void){


}
                                   
