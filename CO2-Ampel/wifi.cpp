/***************************************************************************
  Copyright (c) 2020-2021 Lars Wessels

  This file a part of the "CO2-Ampel" source code.
  https://github.com/lrswss/co2ampel

  Published under Apache License 2.0

***************************************************************************/

#include "wifi.h"
#include "led.h"
#include "logging.h"
#include "utils.h"
#include "rtc.h"

static bool wifiActive = false;
static bool wifiAP = false;
static bool wifiUplink = false;
static bool mdnsStarted = true;
wifiprefs_t wifiSettings;


static bool printWifiSettings(wifiprefs_t *s) {
 #ifdef SETTINGS_DEBUG
  Serial.printf("WebserverTimeout: %d\n", s->webserverTimeout);
  Serial.printf("WebserverAutoOff: %d\n", s->webserverAutoOff);
  Serial.printf("EnableREST: %d\n", s->enableREST);
  Serial.printf("EnableUplink: %d\n", s->enableWLANUplink);
  Serial.printf("Password AP: %s\n", s->wifiApPassword);
  Serial.printf("WiFi SSID: %s\n", s->wifiStaSSID);
  Serial.printf("WiFi password: %s\n", s->wifiStaPassword);
  Serial.printf("CRC: %d\n", s->crc);
#endif
  return true;
}


// set defaults from config.h
static void setDefaults(wifiprefs_t *s) {
  // an explicit memset is required to ensure zeroed padding
  // bytes in struct which would otherwise break CRC
  memset(s, 0, sizeof(*s));

  s->webserverTimeout = WEBSERVER_TIMEOUT_SECS;
#if WEBSERVER_TIMEOUT_SECS > 0 && !defined(ENABLE_REST)
  s->webserverAutoOff = true;
#endif
#ifdef ENABLE_REST
  s->enableREST = true;
#endif
#if defined(ENABLE_WLAN_UPLINK) || defined(ENABLE_REST)
  s->enableWLANUplink = true;
#endif
#ifdef WIFI_AP_PASSWORD
  strncpy(s->wifiApPassword, WIFI_AP_PASSWORD, sizeof(s->wifiApPassword));
#endif
#ifdef WIFI_STA_SSID
  strncpy(s->wifiStaSSID, WIFI_STA_SSID, sizeof(s->wifiStaSSID));
#ifdef WIFI_STA_PASSWORD
  strncpy(s->wifiStaPassword, WIFI_STA_PASSWORD, sizeof(s->wifiStaPassword));
#endif
#endif
  s->crc = crc16((uint8_t *) s, offsetof(wifiprefs_t, crc));
}


static void wifi_mdns() {
  if (!mdnsStarted && !MDNS.begin(MDNS_NAME)) 
    Serial.println(F("WiFi: failed to setup MDNS responder!"));
  else
    mdnsStarted = true;  
}


static bool wifi_init() {
  if (wifiActive)
    return true;
  Serial.println(F("WiFi started."));
  if (WiFi.mode(WIFI_AP_STA)) {
    wifiActive = true;
  } else {
    Serial.println(F("WiFi failed!"));
    logMsg("wifi failed");
    blink_leds(HALF_RING, RED, 250, 2, false);
    delay(750);
  }
  delay(250);
  return wifiActive;
}


bool wifi_start_ap(const char* ssid, const char* pass) {
  char buf[64], ap_ssid[32];

  if (!strlen(ssid)) {
    Serial.println(F("WiFi: failed to start access point, no SSID set!"));
    return false;;
  } else if (strlen(pass) < 8) {
    Serial.println(F("WiFi: failed to start access point, password too short!"));
    return false;
  } else if (!wifiActive && !wifi_init())
    return false;

  save_leds();
  sprintf(ap_ssid, "%s-%s", ssid, systemID().c_str());
  if (WiFi.softAP(ap_ssid, pass)) {
    Serial.printf("WiFi: local AP with SSID %s, IP %s started.\n",
      ap_ssid, WiFi.softAPIP().toString().c_str());
    sprintf(buf, "start access point %s", ap_ssid);
    logMsg(buf);
    wifi_mdns();
    blink_leds(SYSTEM_LEDS, BLUE, 100, 2, true);
    wifiAP = true;
  } else {
    Serial.println(F("WiFi: failed to start local AP!"));
    logMsg("access point failed");
    blink_leds(SYSTEM_LEDS, RED, 250, 2, true);
  }
  delay(1000);
  return wifiAP;
}


static bool wifi_start_sta(const char* ssid, const char* pass, uint8_t timeoutSecs) {
  uint8_t ticks = 0;
  char buf[64];

  if (!strlen(ssid)) {
    Serial.println(F("WiFi: failed to connect to station, no SSID set!"));
    return false;
  } else if (!wifiActive && !wifi_init())
    return false;

  if (wifiUplink && WiFi.status() == WL_CONNECTED)
    return true;

  Serial.printf("WiFi: connecting to SSID %s...", ssid);
  WiFi.begin(ssid, pass);
  save_leds();
  set_leds(SYSTEM_LEDS, BLUE);
  while (WiFi.status() != WL_CONNECTED && (ticks/2) <= timeoutSecs) {
    Serial.print(".");
    Serial.flush();
    delay(500);
    ticks++;
  }
  restore_leds();
  delay(500);
    
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("with IP %s.\n", WiFi.localIP().toString().c_str());
    sprintf(buf, "connect ssid %s, ip %s", ssid, WiFi.localIP().toString().c_str());
    logMsg(buf);
    blink_leds(SYSTEM_LEDS, GREEN, 100, 2, true);
    wifi_mdns();
    wifiSettings.enableWLANUplink = true;
    wifiUplink = true;
  } else {
    Serial.println(F("failed!"));
    blink_leds(SYSTEM_LEDS, RED, 250, 2, true);
    wifiUplink = false;
  }
  delay(1000);
  return wifiUplink;
}


// start or stop local access point
bool wifi_hotspot(bool terminate) {
  if (terminate) {
    if (wifiAP) {
      WiFi.softAPdisconnect(true);
      Serial.println(F("WiFi: local AP stopped."));
      wifiAP = false;
    }
    return wifiAP;
  } else {
    return wifi_start_ap(WIFI_AP_SSID, wifiSettings.wifiApPassword);
  }
}


// check for WiFi uplink
// triggers new connection if reconnect is set
bool wifi_uplink(bool reconnect) {
  if (!reconnect)
    return (WiFi.status() == WL_CONNECTED);
  return wifi_start_sta(wifiSettings.wifiStaSSID, wifiSettings.wifiStaPassword, WIFI_STA_CONNECT_TIMEOUT);
}


void wifi_stop() {
  if (!wifiActive)
    return;
    
  stopNTPSync();
  if (mdnsStarted)
    MDNS.end();
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(1);
  wifiActive = false;
  wifiUplink = false;
  wifiAP = false;
  Serial.println(F("WiFi stopped."));
  Serial.flush();
  logMsg("wifi stopped");
}


void loadWifiSettings() {
  wifiprefs_t buf;

  memset(&wifiSettings, 0, sizeof(wifiSettings));
  setDefaults(&wifiSettings);  // set struct with defaults values from config.h
  loadSettings(&wifiSettings, &buf, offsetof(wifiprefs_t, crc), EEPROM_WIFI_PREFS_ADDR, "WiFi settings");
  printWifiSettings(&wifiSettings);
}


bool saveWifiSettings() {
  if (wifiSettings.crc) { // use an existing crc as flag for a valid settings struct
    wifiSettings.crc = crc16((uint8_t *) &wifiSettings, offsetof(wifiprefs_t, crc));
    return saveSettings(wifiSettings, EEPROM_WIFI_PREFS_ADDR, "WiFi settings") && printWifiSettings(&wifiSettings);
  }
  return false;
}


bool resetWifiSettings() {
  Serial.println(F("Reset WiFi settings."));
  logMsg("reset WiFi settings");
  memset(&wifiSettings, 0, sizeof(wifiSettings));
  setDefaults(&wifiSettings);
  return saveSettings(wifiSettings, EEPROM_WIFI_PREFS_ADDR, "WiFi settings") && printWifiSettings(&wifiSettings);
}
