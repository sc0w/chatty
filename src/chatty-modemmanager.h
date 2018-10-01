/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */


#ifndef __MM_H_INCLUDE__
#define __MM_H_INCLUDE__

#define MM_SERVICE_NAME       "org.freedesktop.ModemManager1"
#define MM_SEND_SMS_TIMEOUT   35000
#define MM_SETTING_TIMEOUT    20000


typedef struct {
  GDBusConnection     *connection;
  GDBusObjectManager  *objectmanager;
  GDBusProxy          *proxy_device;
  GDBusProxy          *proxy_modem;
  GDBusProxy          *proxy_messaging;
  GList               *sms_path;
  gchar               *device_path;
  gchar               *manufacturer;
  gchar               *model;
  gchar               *version;
  gchar               *internal_id;
  gchar               *imei;
  gboolean            device_enabled;
  gboolean            device_blocked;
  gboolean            device_registered;
  gboolean            device_prepared;
  gint                device_type;
  guint               device_state;
} chatty_mm_data_t;


typedef struct {
  gchar     *number;
  gchar     *svc_number;
  GArray    *idents;
  GString   *text;
  gulong    dbid;
  gboolean  read;
  gboolean  binary;
  guint     folder;
  time_t    time_stamp;
} mm_sms_message_t;


enum {
  MM_MODEM_STATE_FAILED        = -1,
  MM_MODEM_STATE_UNKNOWN       = 0,
  MM_MODEM_STATE_INITIALIZING  = 1,
  MM_MODEM_STATE_LOCKED        = 2,
  MM_MODEM_STATE_DISABLED      = 3,
  MM_MODEM_STATE_DISABLING     = 4,
  MM_MODEM_STATE_ENABLING      = 5,
  MM_MODEM_STATE_ENABLED       = 6,
  MM_MODEM_STATE_SEARCHING     = 7,
  MM_MODEM_STATE_REGISTERED    = 8,
  MM_MODEM_STATE_DISCONNECTING = 9,
  MM_MODEM_STATE_CONNECTING    = 10,
  MM_MODEM_STATE_CONNECTED     = 11
} MMModemState;


enum {
  MODULE_SMS_STATE_UNKNOWN,
  MODULE_SMS_STATE_STORED,
  MODULE_SMS_STATE_RECEIVING,
  MODULE_SMS_STATE_RECEIVED,
  MODULE_SMS_STATE_SENDING,
  MODULE_SMS_STATE_SENT
} MMSmsState;


enum {
  MODULE_SMS_PDU_TYPE_UNKNOWN,
  MODULE_SMS_PDU_TYPE_DELIVER,
  MODULE_SMS_PDU_TYPE_SUBMIT,
  MODULE_SMS_PDU_TYPE_STATUS_REPORT
} MMSmsPduType;


enum {
  MM_DEVICE_TYPE_GSM = 1,
  MM_DEVICE_TYPE_CDMA
} e_mm_device_types;

chatty_mm_data_t *chatty_get_mm_data (void);

gboolean chatty_mm_open_device (void);
void chatty_mm_close_device (void);
gboolean chatty_mm_send_sms (gchar* number, gchar *text, gboolean report, guint validity);


#endif
