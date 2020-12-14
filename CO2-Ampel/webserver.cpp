/***************************************************************************
  Copyright (c) 2020 Lars Wessels

  This file a part of the "CO2-Ampel" source code.
  https://github.com/lrswss/co2ampel

  Published under Apache License 2.0

***************************************************************************/

#include "webserver.h"
#include "lorawan.h"
#include "logging.h"
#include "html.h"
#include "utils.h"
#include "led.h"
#include "rtc.h"
#include "mqtt.h"
#include "sensors.h"
#include "wifi.h"
#include "config.h"

ESP8266WebServer webserver(80);
static uint32_t webserverRequestMillis = 0;
static uint16_t webserverTimeout = 0;


static void requirePassword() {
  if (settings.enableAuth && strlen(settings.authUsername) > 4 && strlen(settings.authPassword) > 4)
    if (!webserver.authenticate(settings.authUsername, settings.authPassword))
      return webserver.requestAuthentication();
}


static void setCrossOrigin() {
  webserver.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  webserver.sendHeader(F("Access-Control-Max-Age"), F("600"));
  webserver.sendHeader(F("Access-Control-Allow-Methods"), F("GET,OPTIONS"));
  webserver.sendHeader(F("Access-Control-Allow-Headers"), F("*"));
}


static void sendCORS() {
  setCrossOrigin();
  webserver.sendHeader(F("Access-Control-Allow-Credentials"), F("false"));
  webserver.send(204);
}


// pass sensor readings, system status to web ui as JSON
static void updateUI() {
  StaticJsonDocument<288> JSON;
  char reply[256], buf[16];

  JSON["date"] = getDateString();
  JSON["time"] = getTimeString(false);
  JSON["temperature"] = bme280_temperature;
  if (hasBME280)
    JSON["humidity"] = bme280_humidity;
  else
    JSON["humidity"] = "--";
  JSON["pressure"] = bme280_pressure;
  JSON["co2median"] = scd30_co2ppm;
  JSON["vbat"] = ((int)(getVBAT()*100)) / 100.0;
  JSON["co2status"] = int(co2status);
  JSON["webserverTimeout"] = (webserverTimeout*1000 - (millis()-webserverRequestMillis))/1000;
  JSON["calibrationTimeout"] = scd30_calibrate_countdown;
  JSON["warmupTimeout"] = scd30_warmup_countdown;
#ifdef HAS_LORAWAN_SHIELD
  if (lorawanSettings.enabled && lorawanSession.lmic.devaddr > 0) {
    sprintf(buf, "%08X", lorawanSession.lmic.devaddr);
    JSON["loraDevAddr"] = buf;
    JSON["loraSeqnoUp"] = lorawanSession.lmic.seqnoUp;
  } else {
    JSON["loraDevAddr"] = "--------";
    JSON["loraSeqnoUp"] = "--";
  }
#endif
#ifdef HAS_LORAWAN_SHIELD
  JSON["otaa"] = int(lorawanSettings.useOTAA);
#else
  JSON["otaa"] = int(0);
#endif
  serializeJson(JSON, reply);
  webserver.send(200, F("application/json"), reply);
}


// send sensor readings on RESTful request on /readings
static void handleREST() {
  StaticJsonDocument<128> JSON;
  char reply[128];

  setCrossOrigin();
  if (co2status <= ALARM) {
    JSON["co2median"] = scd30_co2ppm;
    JSON["temperature"] = bme280_temperature;
    if (hasBME280)
      JSON["humidity"] = bme280_humidity;
    JSON["pressure"] = bme280_pressure;
  }
  JSON["vbat"] = ((int)(getVBAT()*100)) / 100.0;
  JSON["co2status"] = statusNames[co2status];

  serializeJson(JSON, reply);
  webserver.send(200, F("application/json"), reply);
}


// start local AP and webserver for OTA firmware
// updates and log file download from LittleFS
void webserver_start(uint16_t timeout) {
  char buf[64];

  webserverTimeout = timeout;
  if (webserverRequestMillis)
    return;
  webserverRequestMillis = millis();

  // send main page
  webserver.on("/", HTTP_GET, []() {
    String html = FPSTR(HEADER_html);
    html += FPSTR(ROOT_html);
    html.replace("__SYSTEMID__", systemID());
    html += FPSTR(FOOTER_html);
    html.replace("__FIRMWARE__", String(FIRMWARE_VERSION));
    webserver.send(200, "text/html", html);
    webserverRequestMillis = millis();
  });

  // AJAX request from main page to update readings
  webserver.on("/ui", HTTP_GET, updateUI);

  // handle RESTful requests
  webserver.on(F("/readings"), HTTP_OPTIONS, sendCORS);
  webserver.on(F("/readings"), HTTP_GET, handleREST);

  // show page with log files
  webserver.on("/logs", HTTP_GET, []() {
    FSInfo fs_info;

    logMsg("show logs");
    LittleFS.info(fs_info);
    uint32_t freeBytes = fs_info.totalBytes * 0.95 - fs_info.usedBytes;
    String html = FPSTR(HEADER_html);
    html += FPSTR(LOGS_HEADER_html);
    html.replace("__BYTES_FREE__", String(freeBytes / 1024));
    html += listDirHTML("/");
    html += FPSTR(LOGS_FOOTER_html);
    html += FPSTR(FOOTER_html);
    html.replace("__FIRMWARE__", String(FIRMWARE_VERSION));
    webserver.send(200, "text/html", html);
    Serial.println(F("Show log files."));
    webserverRequestMillis = millis();
  });

  // handle request to update firmware
  webserver.on("/update", HTTP_GET, []() {
    requirePassword();
    String html = FPSTR(HEADER_html);
    html += FPSTR(UPDATE_html);
    html += FPSTR(FOOTER_html);
    html.replace("__FIRMWARE__", String(FIRMWARE_VERSION));
    html.replace("__DISPLAY__", "display:none;");
    webserver.send(200, "text/html", html);
    Serial.println(F("Show update page."));
    webserverRequestMillis = millis();
  });

  // query system setup/configuration
  webserver.on("/setup", HTTP_GET, []() {
    String html;
#ifdef HAS_LORAWAN_SHIELD
    html += "1,";
#else
    html += "0,";
#endif
    html += settings.enableLogging ? "1" : "0";
    webserver.send(200, "text/html", html);
  });

  // delete all log files
  webserver.on("/rmlogs", HTTP_GET, []() {
    requirePassword();
    logMsg("remove logs");
    removeLogs();
    webserver.send(200, "text/plain", "OK");
    webserverRequestMillis = millis();
  });

  // handle firmware upload
  webserver.on("/update", HTTP_POST, []() {
    requirePassword();
    String html = FPSTR(HEADER_html);
    if (Update.hasError()) {
      html += FPSTR(UPDATE_ERR_html);
      blink_leds(QUARTER_RING, RED, 250, 4, true);
      logMsg("ota failed");
    } else {
      html += FPSTR(UPDATE_OK_html);
      blink_leds(QUARTER_RING, GREEN, 250, 2, true);
      logMsg("ota successful");
    }
    html += FPSTR(FOOTER_html);
    html.replace("__FIRMWARE__", String(FIRMWARE_VERSION));
    webserver.send(200, "text/html", html);
  }, []() {
    HTTPUpload& upload = webserver.upload();
    if (upload.status == UPLOAD_FILE_START) {
      save_leds();
      set_leds(QUARTER_RING, CYAN);
      Serial.println(F("Starting OTA update..."));
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if (!Update.begin(maxSketchSpace)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      webserverRequestMillis = millis();
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.println(F("Update successful!"));
      } else {
        Update.printError(Serial);
      }
    }
  });

  webserver.on("/tickle", HTTP_GET, []() {
    webserverRequestMillis = millis();
    webserver.send(200, "text/plain", "OK");
  });

  webserver.on("/sendlogs", HTTP_GET, []() {
    logMsg("send all logs");
    sendAllLogs();
  });

  webserver.on("/restart", HTTP_GET, []() {
    webserver.send(200, "text/plain", "OK");
    resetSystem();
  });

  webserver.on("/reset/config", HTTP_GET, []() {
    requirePassword();
    if (resetGeneralSettings())
      webserver.send(200, "text/plain", "OK");
    else
      webserver.send(200, "text/plain", "ERR");
  });

#ifdef HAS_LORAWAN_SHIELD
  webserver.on("/reset/lorawan", HTTP_GET, []() {
    requirePassword();
    if (resetLoRaWANSettings() && resetLoRaWANSession)
      webserver.send(200, "text/plain", "OK");
    else
      webserver.send(200, "text/plain", "ERR");
  });
#endif

  webserver.on("/reset/network", HTTP_GET, []() {
    requirePassword();
    if (resetMQTTSettings() && resetWifiSettings())
      webserver.send(200, "text/plain", "OK");
    else
      webserver.send(200, "text/plain", "ERR");
  });

  webserver.on("/reset/full", HTTP_GET, []() {
    requirePassword();
    resetGeneralSettings();
    resetWifiSettings();
    resetMQTTSettings();
#ifdef HAS_LORAWAN_SHIELD
    resetLoRaWANSettings();
    resetLoRaWANSession();
#endif
    webserver.send(200, "text/plain", "OK");
  });

  // trigger SCD30 calibration
  webserver.on("/calibrate", HTTP_GET, []() {
    requirePassword();
    scd30_calibrate(SCD30_CALIBRATION_SECS);
    webserver.send(200, "text/plain", "OK");
  });

  // show general sensor settings
  webserver.on("/config", HTTP_GET, []() {
    String html;
    html += FPSTR(HEADER_html);
    html += FPSTR(SETTINGS_html);
    html += FPSTR(FOOTER_html);
    html.replace("__FIRMWARE__", String(FIRMWARE_VERSION));    
    html.replace("__CO2_MEDIUM__", String(settings.co2MediumThreshold));
    html.replace("__CO2_HIGH__", String(settings.co2HighThreshold));
    html.replace("__CO2_ALARM__", String(settings.co2AlarmThreshold));
    html.replace("__INTERVALMIN__", String(SCD30_INTERVAL_MIN_SECS));
    html.replace("__INTERVAL__", String(settings.co2ReadingInterval));
    html.replace("__SAMPLES__", String(settings.co2MedianSamples));
    html.replace("__HYSTERESIS__", String(settings.co2ThresholdHysteresis));
    html.replace("__NOOPSTART__", String(settings.beginSleep));
    html.replace("__NOOPEND__", String(settings.endSleep));
    html.replace("__USERNAME__", String(settings.authUsername));
    html.replace("__PASSWORD__", String(settings.authPassword));
    html.replace("__ALTITUDE__", String(settings.altitude));
    html.replace("__LOGINTERVAL__", String(settings.loggingInterval));
    if (settings.enableNOOP && settings.beginSleep != settings.endSleep)
      html.replace("__NOOP__", "checked");
    else
      html.replace("__NOOP__", "");
    if (settings.enableAuth)
      html.replace("__AUTH__", "checked");
    else
      html.replace("__AUTH__", "");
    if (settings.enableLogging)
      html.replace("__LOGGING__", "checked");
    else
      html.replace("__LOGGING__", "");
    if (settings.medianFilter)
      html.replace("__MEDIANFILTER__", "checked");
    else
      html.replace("__MEDIANFILTER__", "");

    webserver.send(200, "text/html", html);
    Serial.println(F("Show general settings."));
    webserverRequestMillis = millis();
  });

  // save general settings
  webserver.on("/config", HTTP_POST, []() {
    requirePassword();
    logMsg("webui save general prefs");
    if (webserver.arg("co2medium").toInt() >= 600 &&
        webserver.arg("co2medium").toInt() <= 1000)
      settings.co2MediumThreshold = webserver.arg("co2medium").toInt();
    if (webserver.arg("co2high").toInt() > 1000 &&
        webserver.arg("co2high").toInt() <= 1500)
      settings.co2HighThreshold = webserver.arg("co2high").toInt();
    if (webserver.arg("co2alarm").toInt() > 1500 &&
        webserver.arg("co2alarm").toInt() <= 2500)
      settings.co2AlarmThreshold = webserver.arg("co2alarm").toInt();
    if (webserver.arg("interval").toInt() >= SCD30_INTERVAL_MIN_SECS &&
        webserver.arg("interval").toInt() <= SCD30_INTERVAL_MAX_SECS)
      settings.co2ReadingInterval = webserver.arg("interval").toInt();   
    if (webserver.arg("samples").toInt() >= 3 &&
        webserver.arg("samples").toInt() <= 15)
      settings.co2MedianSamples = webserver.arg("samples").toInt();
    if (webserver.arg("hysteresis").toInt() >= 20 &&
        webserver.arg("hysteresis").toInt() <= 100)
      settings.co2ThresholdHysteresis = webserver.arg("hysteresis").toInt();
    if (webserver.arg("altitude").toInt() >= 0 &&
        webserver.arg("altitude").toInt() <= 4000)
      settings.altitude = webserver.arg("altitude").toInt();
    if (webserver.arg("noopstart").toInt() >= 0 && webserver.arg("noopstart").toInt() <= 23)
      settings.beginSleep = webserver.arg("noopstart").toInt();
    if (webserver.arg("noopend").toInt() >= 0 && webserver.arg("noopend").toInt() <= 23)
      settings.endSleep = webserver.arg("noopend").toInt();
    if (webserver.arg("username").length() >= 3 && webserver.arg("username").length() <= 15)
      strncpy(settings.authUsername, webserver.arg("username").c_str(), 15);
    if (webserver.arg("password").length() >= 6 && webserver.arg("password").length() <= 15)
      strncpy(settings.authPassword, webserver.arg("password").c_str(), 15);
    if (webserver.arg("loginterval").toInt() >= 60 && webserver.arg("loginterval").toInt() <= 900)
      settings.loggingInterval = webserver.arg("loginterval").toInt();

    if (webserver.arg("medianfilter") == "on")
      settings.medianFilter = true;
    else
      settings.medianFilter = false; 
    if (webserver.arg("noop") == "on") {
      settings.enableNOOP = true;
      if (checkNOOPTime(settings.beginSleep, settings.endSleep))
        webserver_settimeout(WEBSERVER_TIMEOUT_NOOP);
    } else {
      if (settings.enableNOOP)
        webserver_settimeout(WEBSERVER_TIMEOUT_SECS);
      settings.enableNOOP = false;
    }
    if (webserver.arg("auth") == "on")
      settings.enableAuth = true;
    else
      settings.enableAuth = false;
    if (webserver.arg("logging") == "on")
      settings.enableLogging = true;
    else
      settings.enableLogging = false;
      
    if (saveGeneralSettings())
      webserver.sendHeader("Location", "/config?saved", true);
    else
      webserver.sendHeader("Location", "/config?failed", true);
    webserver.send(302, "text/plain", "");
    webserverRequestMillis = millis();
  });

  // show network settings (WiFi and MQTT)
  webserver.on("/network", HTTP_GET, []() {
    String html;
    
    requirePassword();
    html += FPSTR(HEADER_html);
    html += FPSTR(NETWORK_html);
    html += FPSTR(FOOTER_html);
    html.replace("__FIRMWARE__", String(FIRMWARE_VERSION));
    html.replace("__APPASSWORD__", String(wifiSettings.wifiApPassword));
    html.replace("__WEBTIMEOUT__", String(wifiSettings.webserverTimeout));
    html.replace("__WEBTIMEOUTMIN__", String(WEBSERVER_TIMEOUT_MIN_SECS));
    html.replace("__STASSID__", String(wifiSettings.wifiStaSSID));
    html.replace("__STAPASSWORD__", String(wifiSettings.wifiStaPassword));
    html.replace("__MQTTBROKER__", String(mqttSettings.broker));
    html.replace("__MQTTTOPIC__", String(mqttSettings.topic));
    html.replace("__MQTTUSERNAME__", String(mqttSettings.username));
    html.replace("__MQTTPASSWORD__", String(mqttSettings.password));
    html.replace("__MQTTINTERVAL__", String(mqttSettings.pushInterval));

    if (wifiSettings.webserverAutoOff)
      html.replace("__WEBAUTOOFF__", "checked");
    else
      html.replace("__WEBAUTOOFF__", "");
    if (wifiSettings.enableWLANUplink)
      html.replace("__WLAN__", "checked");
    else
      html.replace("__WLAN__", "");  
    if (mqttSettings.enabled)
      html.replace("__MQTT__", "checked");
    else
      html.replace("__MQTT__", "");
    if (mqttSettings.enableAuth)
      html.replace("__MQTTAUTH__", "checked");
    else
      html.replace("__MQTTAUTH__", "");
    if (!mqttSettings.enableJSON)
      html.replace("__MQTTJSON__", "checked");
    else
      html.replace("__MQTTJSON__", "");    
      
    webserver.send(200, "text/html", html);
    Serial.println(F("Show network settings."));
    webserverRequestMillis = millis();    
  });

  webserver.on("/network", HTTP_POST, []() {

    requirePassword();
    if (webserver.arg("appassword").length() >= 8 && webserver.arg("appassword").length() <= 31)
      strncpy(wifiSettings.wifiApPassword, webserver.arg("appassword").c_str(), 31);   
    if (webserver.arg("webtimeout").toInt() >= WEBSERVER_TIMEOUT_MIN_SECS &&
        webserver.arg("webtimeout").toInt() <= WEBSERVER_TIMEOUT_MAX_SECS) {
      wifiSettings.webserverTimeout = webserver.arg("webtimeout").toInt();
      if (!settings.enableNOOP)
        webserver_settimeout(wifiSettings.webserverTimeout);
    }     
    if (webserver.arg("stassid").length() >= 4 && webserver.arg("stassid").length() <= 31)
      strncpy(wifiSettings.wifiStaSSID, webserver.arg("stassid").c_str(), 31);
    if (webserver.arg("stapassword").length() >= 8 && webserver.arg("stapassword").length() <= 31)
      strncpy(wifiSettings.wifiStaPassword, webserver.arg("stapassword").c_str(), 31);  
    if (webserver.arg("mqttbroker").length() >= 4 && webserver.arg("mqttbroker").length() <= 63)
      strncpy(mqttSettings.broker, webserver.arg("mqttbroker").c_str(), 63);
    if (webserver.arg("mqtttopic").length() >= 4 && webserver.arg("mqttbroker").length() <= 63)
      strncpy(mqttSettings.topic, webserver.arg("mqtttopic").c_str(), 63);
    if (webserver.arg("mqttuser").length() >= 4 && webserver.arg("mqttuser").length() <= 31)
      strncpy(mqttSettings.username, webserver.arg("mqttuser").c_str(), 31);
    if (webserver.arg("mqttpassword").length() >= 4 && webserver.arg("mqttpassword").length() <= 31)
      strncpy(mqttSettings.password, webserver.arg("mqttpassword").c_str(), 31);
    if (webserver.arg("mqttinterval").toInt() >= settings.co2ReadingInterval &&
        webserver.arg("mqttinterval").toInt() <= 900)
      mqttSettings.pushInterval = webserver.arg("mqttinterval").toInt();

    if (webserver.arg("webserverautooff") == "on")
      wifiSettings.webserverAutoOff = true;
    else
      wifiSettings.webserverAutoOff = false;
    if (webserver.arg("wlan") == "on")
      wifiSettings.enableWLANUplink = true;
    else
      wifiSettings.enableWLANUplink = false;
    if (webserver.arg("mqtt") == "on")
      mqttSettings.enabled = true;
    else
      mqttSettings.enabled = false;
    if (webserver.arg("mqttauth") == "on")
      mqttSettings.enableAuth = true;
    else
      mqttSettings.enableAuth = false;
    if (webserver.arg("mqttjson") == "on")
      mqttSettings.enableJSON = false;
    else
      mqttSettings.enableJSON = true;

    if (saveWifiSettings() && saveMQTTSettings())
      webserver.sendHeader("Location", "/network?saved", true);
    else
      webserver.sendHeader("Location", "/network?failed", true);
    webserver.send(302, "text/plain", "");    
    webserverRequestMillis = millis();
  });
  
#ifdef HAS_LORAWAN_SHIELD
  webserver.on("/lorawan", HTTP_GET, []() {
    String html;
    byte deveui[16];
    char str[33];
    char devAddrStr[9] = "00000000";

    requirePassword();
    html += FPSTR(HEADER_html);
    html += FPSTR(LORAWAN_html);
    html += FPSTR(FOOTER_html);
    html.replace("__FIRMWARE__", String(FIRMWARE_VERSION));

    os_getDevEui(deveui);
    array2string(deveui, 8, str, true);
    html.replace("__TXINTERVAL__", String(lorawanSettings.txInterval));
    html.replace("__DEVEUI__", String(str));
    html.replace("__DRSEND__", String(lorawanSettings.drSend));
    html.replace("__DRJOIN__", String(lorawanSettings.drJoin));
    array2string(lorawanSettings.appEui, 8, str, true);
    html.replace("__APPEUI__", String(str));
    array2string(lorawanSettings.appKey, 16, str, false);
    html.replace("__APPKEY__", String(str));
    if (lorawanSettings.devAddr > 0)
      sprintf(devAddrStr, "%08X", lorawanSettings.devAddr); // uint32_t
    html.replace("__DEVADDR__", String(devAddrStr));
    array2string(lorawanSettings.nwksKey, 16, str, false);
    html.replace("__NWKSKEY__", String(str));
    array2string(lorawanSettings.appsKey, 16, str, false);
    html.replace("__APPSKEY__", String(str));
    if (lorawanSettings.enabled)
      html.replace("__ENABLED__", "checked");
    else
      html.replace("__ENABLED__", "");
    if (lorawanSettings.useTTN)
      html.replace("__TTN__", "checked");
    else
      html.replace("__TTN__", "");
    if (lorawanSettings.useOTAA) {
      html.replace("__OTAA__", "checked");
      html.replace("__ABP__", "");
    } else {
      html.replace("__ABP__", "checked");
      html.replace("__OTAA__", "");
    }

    webserver.send(200, "text/html", html);
    Serial.println(F("Show LoRaWAN settings."));
    webserverRequestMillis = millis();
  });

  // save LoRaWAN settings
  webserver.on("/lorawan", HTTP_POST, []() {
    char appeuiStr[17], appkeyStr[33], devAddrStr[9], nwkskeyStr[33], appskeyStr[33];
    bool updateSession = false;

    requirePassword();
    logMsg("save lorawan prefs");
    if (webserver.arg("txinterval").toInt() >= 60 && webserver.arg("txinterval").toInt() <= 3600)
      lorawanSettings.txInterval = webserver.arg("txinterval").toInt();    
    if (webserver.arg("appeui").length() == sizeof(lorawanSettings.appEui)*2) {
      array2string(lorawanSettings.appEui, 8, appeuiStr, true); // save old key for strcmp()
      string2array(webserver.arg("appeui").c_str(),
        sizeof(lorawanSettings.appEui), lorawanSettings.appEui, true);
      if (strcmp(webserver.arg("appeui").c_str(), appeuiStr))
        lorawanSession.lmic.devaddr = 0; // appeui changed, trigger rejoin
    }
    if (webserver.arg("appkey").length() == sizeof(lorawanSettings.appKey)*2) {
      array2string(lorawanSettings.appKey, 16, appkeyStr, false);
      string2array(webserver.arg("appkey").c_str(),
        sizeof(lorawanSettings.appKey), lorawanSettings.appKey, false);
      if (strcmp(webserver.arg("appkey").c_str(), appkeyStr))
        lorawanSession.lmic.devaddr = 0; // appkey changed, trigger rejoin
    }
    if (webserver.arg("devaddr").length() == sizeof(lorawanSettings.devAddr)*2) {
      lorawanSettings.devAddr = hex2num(webserver.arg("devaddr").c_str());
      if (lorawanSettings.devAddr != lorawanSession.lmic.devaddr) // addr changed?
        updateSession = true;
    }
    if (webserver.arg("nwkskey").length() == sizeof(lorawanSettings.nwksKey)*2) {
      string2array(webserver.arg("nwkskey").c_str(),
        sizeof(lorawanSettings.nwksKey), lorawanSettings.nwksKey, false);
      array2string(lorawanSession.lmic.nwkKey, 16, nwkskeyStr, false);
      if (strcmp(webserver.arg("nwkskey").c_str(), nwkskeyStr))
        updateSession = true;
    }
    if (webserver.arg("appskey").length() == sizeof(lorawanSettings.appsKey)*2) {
      string2array(webserver.arg("appskey").c_str(),
        sizeof(lorawanSettings.appsKey), lorawanSettings.appsKey, false);
      array2string(lorawanSession.lmic.artKey, 16, appskeyStr, false);
      if (strcmp(webserver.arg("appkey").c_str(), appkeyStr))
        updateSession = true;
    }
    if (webserver.arg("drsend").toInt() >= 0 && webserver.arg("drsend").toInt() <= 5) {
      lorawanSettings.drSend = webserver.arg("drsend").toInt();
      if (LMIC.datarate != lorawanSettings.drSend)
        LMIC_setDrTxpow(lorawanSettings.drSend, LORAWAN_TXPOWER);
    }
    if (webserver.arg("drjoin").toInt() >= 0 && webserver.arg("drjoin").toInt() <= 5)
      lorawanSettings.drJoin = webserver.arg("drjoin").toInt();

    if (webserver.arg("enabled") == "on") {
      lorawanSettings.enabled = true;
    } else
      lorawanSettings.enabled = false;
    if (webserver.arg("otaa") == "on" ) {
      if (!lorawanSettings.useOTAA)
        lorawanSession.lmic.devaddr = 0;
      lorawanSettings.useOTAA = true;
    } else {
      lorawanSettings.useOTAA = false;
    }
    if (webserver.arg("ttn") == "on")
      lorawanSettings.useTTN = true;
    else
      lorawanSettings.useTTN = false;
       
    if (saveLoRaWANSettings(updateSession))
      webserver.sendHeader("Location", "/lorawan?saved", true);
    else
      webserver.sendHeader("Location", "/lorawan?failed", true);
    webserver.send(302, "text/plain", "");    
    webserverRequestMillis = millis();
  });
#endif

  webserver.onNotFound([]() {
    if (!handleSendFile(webserver.uri())) {
      webserver.send(404, "text/plain", "Error 404: file not found");
    }
  });

  if (wifiSettings.webserverAutoOff || co2status == NOOP) {
    Serial.print(F("Starting webserver with "));
    Serial.print(timeout);
    Serial.println(F(" secs. timeout..."));
    sprintf(buf, "webserver, timeout %d secs", timeout);
    logMsg(buf);
  } else {
    Serial.print(F("Starting webserver (without timeout)..."));
    logMsg("webserver, no timeout");
  }
  webserver.begin();
}


// change webserver timeout and reset its timer
void webserver_settimeout(uint16_t timeoutSecs) {
  if (webserverTimeout != timeoutSecs) {
    Serial.printf("Set webserver timeout to %d secs.\n", timeoutSecs);
    webserverTimeout = timeoutSecs;
    webserver_tickle();
  }
}


// reset timeout countdown
void webserver_tickle() {
  webserverRequestMillis = millis();
}


// returns millis since last request
uint32_t webserver_idle() {
  return (millis() - webserverRequestMillis);
}


// returns false if webserver timeout has not been reached
// if force is true webserver will be stopped if running
bool webserver_stop(bool force) {
  if (!wifiSettings.webserverAutoOff && co2status != NOOP)
    return false;
    
  if (!force && (millis() - webserverRequestMillis) < (webserverTimeout*1000))
    return false;

  if (webserverRequestMillis > 0) {
    webserver.stop();
    webserverRequestMillis = 0;
    set_leds(SYSTEM_LED1, 0);
    logMsg("webserver off");
    Serial.println(F("Webserver stopped."));
  }
  return true;
}
