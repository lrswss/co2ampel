/***************************************************************************
  Copyright (c) 2020 Lars Wessels

  This file a part of the "CO2-Ampel" source code.
  https://github.com/lrswss/co2ampel

  Published under Apache License 2.0
  
***************************************************************************/

#ifndef _WIFI_H
#define _WIFI_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h> 
#include "config.h"

#define WIFI_AP_SSID "CO2-Ampel"
#define WIFI_STA_CONNECT_TIMEOUT 10
#define MDNS_NAME "ampel"  // ampel.local
#define EEPROM_WIFI_PREFS_ADDR 0x100

typedef struct {
  bool webserverAutoOff;
  uint16_t webserverTimeout;
  char wifiApPassword[32];  
  bool enableWLANUplink;
  char wifiStaSSID[32];
  char wifiStaPassword[32];
  uint16_t crc;
} wifiprefs_t;

extern wifiprefs_t wifiSettings;

bool wifi_hotspot(bool terminate);
bool wifi_uplink(bool reconnect);
void wifi_stop();
void loadWifiSettings();
bool saveWifiSettings();
bool resetWifiSettings();

#endif
