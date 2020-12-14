/***************************************************************************
  Copyright (c) 2020 Lars Wessels

  This file a part of the "CO2-Ampel" source code.
  https://github.com/lrswss/co2ampel

  Published under Apache License 2.0

***************************************************************************/

#ifndef _CONFIG_H
#define _CONFIG_H

#define FIRMWARE_VERSION 100

// https://www.umweltbundesamt.de/sites/default/files/medien/pdfs/kohlendioxid_2008.pdf 
// thresholds set according to table 3 on page 1366
#define CO2_MEDIUM_THRESHOLD 800  // below green, above yellow
#define CO2_HIGH_THRESHOLD 1400  // above red
#define CO2_ALARM_THRESHOLD 2000  // above red blinking
#define CO2_THRESHOLD_HYSTERESIS 50

// measurement interval for air sensor and
// number of samples used for CO2 median reading
#define SCD30_INTERVAL_SECS 10
#define SCD30_NUM_SAMPLES_MEDIAN 6
//#define SCD30_DEBUG

// altitude compensation for air sensor
#define ALTITUDE_ABOVE_SEELEVEL 125

// logging to local flash filesystem (LittleFS)
#define ENABLE_LOGGING
#define LOGGING_INTERVAL_SECS 300

// set credentials to optionally protect settings, 
// calibration and firmware update in webinterface
// optionally enable serial debugging messages for settings
//#define ENABLE_AUTH
#define SETTINGS_USERNAME "admin"
#define SETTINGS_PASSWORD "__secret__"
//#define SETTINGS_DEBUG

// timeout for local webserver (set to 0 to disable)
#define WEBSERVER_TIMEOUT_SECS 600

// credentials for local access point (min. 8 characters!)
#define WIFI_AP_PASSWORD "__secret__"

// credentials for internet connection (wifi uplink)
// used for RTC sync (required at least once) and MQTT messages
//#define ENABLE_WLAN_UPLINK
#define WIFI_STA_SSID "MyWLAN"
#define WIFI_STA_PASSWORD "__secret__""

// offline time to save battery (no leading '0')
//#define ENABLE_NOOP
#define BEGIN_SLEEP_HOUR 22
#define END_SLEEP_HOUR 7

// publish reading with MQTT (if local WLAN is preset/available)
//#define ENABLE_MQTT
#define MQTT_PUSH_INTERVAL_SECS 60
#define MQTT_BROKER "10.1.30.39"
#define MQTT_TOPIC "co2ampel"
#define MQTT_PUSH_JSON
//#define MQTT_USER "ampel"
//#define MQTT_PASS "__secret__"

// uncomment HAS_LORAWAN_SHIELD to compile support
// recommended shield for a Wemos D1: https://github.com/hallard/WeMos-Lora
//#define HAS_LORAWAN_SHIELD
//#define ENABLE_LORAWAN
#define LORAWAN_TX_INTERVAL_SECS 300
#define LORAWAN_DRJOIN DR_SF10
#define LORAWAN_DRSEND DR_SF8
#define LORAWAN_USETTN
#define LORAWAN_NETID 0x01  // test/experimental network
// values for LORAWAN_DEVEUI, LORAWAN_APPEUI are LSB all other MSB
#define LORAWAN_USEOTAA
#define LORAWAN_DEVEUI { 0xE1, 0x3A, 0x5F, 0x19, 0x27, 0x74, 0x9E, 0x01 }
#define LORAWAN_APPEUI { 0x22, 0x14, 0x12, 0xD0, 0x7E, 0xD5, 0xB3, 0x70 }
#define LORAWAN_APPKEY { 0x68, 0xA4, 0x10, 0x55, 0xA9, 0x97, 0x3A, 0x71, 0xE2, 0x8E, 0x40, 0xD3, 0xC0, 0x05, 0x22, 0x90 }
//#define LORAWAN_DEVADDR 0x26103EFC
//#define LORAWAN_NWKSKEY { 0xA3, 0x39, 0xBF, 0xC8, 0x53, 0x60, 0x4E, 0x21, 0x1F, 0x26, 0x33, 0xF0, 0x87, 0x5E, 0x80, 0x30 }
//#define LORAWAN_APPSKEY { 0xC4, 0xE4, 0x14, 0xF0, 0x28, 0x7F, 0x1C, 0x77, 0xE2, 0x97, 0x4A, 0x88, 0x2B, 0x76, 0xF9, 0xC2 }
#define LORAWAN_DEBUG
#define LORAWAN_OBSCURE_KEYS

// adjust according to battery voltage divider 
// (220k from VBAT to A0) on wemos D1 shield
#define VBAT_ADJUST 4.205

#endif
