/***************************************************************************
  Copyright (c) 2020 Lars Wessels

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
wifiprefs_t wifiSettings, wifiSettingsDefault;


static bool printWifiSettings(wifiprefs_t *s) {
 #ifdef SETTINGS_DEBUG
  Serial.printf("WebserverTimeout: %d\n", s->webserverTimeout);
  Serial.printf("WebserverAutoOff: %d\n", s->webserverAutoOff);
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
#if WEBSERVER_TIMEOUT_SECS > 0
  s->webserverAutoOff = true;
#endif
#ifdef ENABLE_WLAN_UPLINK
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
}


static void wifi_mdns() {
  if (!mdnsStarted && !MDNS.begin(MDNS_NAME)) 
    Serial.println(F("WiFi: failed to setup MDNS responder!"));
  else
    mdnsStarted = true;  
}


static void wifi_init() {
  if (wifiActive)
    return;
  Serial.println(F("WiFi started."));
  WiFi.mode(WIFI_AP_STA);
  wifiActive = true;
  delay(500);
}


static void wifi_start_ap(const char* ssid, const char* pass) {
  char buf[64], ap_ssid[32];

  if (!strlen(ssid)) {
    Serial.println(F("WiFi: failed to start access point, no SSID set!"));
    return;
  } else if (strlen(pass) < 8) {
    Serial.println(F("WiFi: failed to start access point, password too short!"));
    return;
  } else if (!wifiActive)
    wifi_init();
    
  sprintf(ap_ssid, "%s-%s", ssid, systemID().c_str());
  WiFi.softAP(ap_ssid, pass);
  Serial.printf("WiFi: local AP with SSID %s, IP %s started.\n", 
    ap_ssid, WiFi.softAPIP().toString().c_str());
  sprintf(buf, "start access point %s", ap_ssid);
  wifiAP = true;
  wifi_mdns();
  logMsg(buf);
  delay(500);
}


static bool wifi_start_sta(const char* ssid, const char* pass, uint8_t timeoutSecs) {
  uint8_t ticks = 0;
  char buf[64];

  if (!strlen(ssid)) {
    Serial.println(F("WiFi: failed to connect to station, no SSID set!"));
    return false;
  } else if (!wifiActive)
    wifi_init();

  if (wifiUplink && WiFi.status() == WL_CONNECTED)
    return true;

  Serial.printf("WiFi: connecting to SSID %s", ssid);
  WiFi.begin(ssid, pass);
  save_leds();
  set_leds(SYSTEM_LED1, BLUE);
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
    wifi_mdns();
    wifiUplink = true;
    wifiSettings.enableWLANUplink = true;
    logMsg(buf);
    delay(500);
    return wifiUplink;

  } else {
    Serial.println(F("failed!"));
    blink_leds(SYSTEM_LED1, RED, 250, 2, true);
    wifiUplink = false;
    delay(1000);
    return wifiUplink;
  }
}


// start or stop local access point
void wifi_hotspot(bool terminate) {
  if (terminate) {
    if (wifiAP) {
      WiFi.softAPdisconnect(true);
      Serial.println(F("WiFi: local AP stopped."));
      wifiAP = false;
    }
  } else {
    wifi_start_ap(WIFI_AP_SSID, wifiSettings.wifiApPassword);
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
  delay(500);
}


void loadWifiSettings() {
  wifiprefs_t buf;
  
  setDefaults(&wifiSettings);  // set struct with defaults values from config.h
  memcpy((void*)&wifiSettingsDefault, &wifiSettings, sizeof(wifiSettings)); // keep a copy for reset
  loadSettings(&wifiSettings, &buf, offsetof(wifiprefs_t, crc), EEPROM_WIFI_PREFS_ADDR, "WiFi settings");
  printWifiSettings(&wifiSettings);
}


bool saveWifiSettings() {
  wifiSettings.crc = crc16((uint8_t *) &wifiSettings, offsetof(wifiprefs_t, crc));
  return saveSettings(wifiSettings, EEPROM_WIFI_PREFS_ADDR, "WiFi settings") &&   printWifiSettings(&wifiSettings);
}


bool resetWifiSettings() {
  memcpy((void*)&wifiSettings, &wifiSettingsDefault, sizeof(wifiSettings)); // use copy with defaults
  return resetSettings(wifiSettings, EEPROM_WIFI_PREFS_ADDR, "WiFi settings") && printWifiSettings(&wifiSettings);

}
