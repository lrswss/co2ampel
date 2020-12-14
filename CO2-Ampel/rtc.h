/***************************************************************************
  Copyright (c) 2020 Lars Wessels

  This file a part of the "CO2-Ampel" source code.
  https://github.com/lrswss/co2ampel

  Published under Apache License 2.0

***************************************************************************/

#ifndef _TIME_H
#define _TIME_H

#include <Arduino.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Time.h>
#include <TimeLib.h>
#include <Timezone.h>
#include "RTClib.h"
#include "uEEPROMLib.h"
#include "sensors.h"
#include "logging.h"
#include "utils.h"
#include "config.h"

#define NTP_ADDRESS "de.pool.ntp.org"
#define EEPROM_SYSTEM_PREFS_ADDR 0x10

#if SCD30_INTERVAL_SECS < SCD30_INTERVAL_MIN_SECS
#define SCD30_INTERVAL_SECS SCD30_INTERVAL_MIN_SECS
#endif
#if SCD30_INTERVAL_SECS > SCD30_INTERVAL_MAX_SECS
#define SCD30_INTERVAL_SECS SCD30_INTERVAL_MAX_SECS
#endif
#if SCD30_NUM_SAMPLES_MEDIAN > 15
#define SCD30_NUM_SAMPLES_MEDIAN 15
#endif
#if SCD30_NUM_SAMPLES_MEDIAN < 3
#define SCD30_NUM_SAMPLES_MEDIAN 3
#endif

extern RTC_DS3231 rtc;
extern Timezone CE;
extern bool rtcOK;


typedef struct {
  float scd30TempOffset;
  uint16_t co2MediumThreshold;
  uint16_t co2HighThreshold;
  uint16_t co2AlarmThreshold;
  uint16_t co2ThresholdHysteresis;
  uint16_t co2ReadingInterval;
  uint16_t co2MedianSamples;
  bool medianFilter;
  bool enableNOOP;
  uint8_t beginSleep;
  uint8_t endSleep;
  char authUsername[16];
  char authPassword[16];
  bool enableAuth;
  bool enableLogging;
  uint16_t loggingInterval;
  uint16_t altitude;
  uint16_t crc;
} sysprefs_t;

extern sysprefs_t settings;
extern uEEPROMLib rtceeprom;

void rtc_init();
void rtc_temperature();
char* getDateString();
char* getTimeString(bool showsecs);
int8_t getHourLocal();
bool startNTPSync();
void stopNTPSync();
void loadGeneralSettings();
bool saveGeneralSettings();
bool resetGeneralSettings();


// template load struct with settings from EEPROM
template <typename T, typename S>
void loadSettings(T *t, S *s, uint8_t crclen, uint16_t addr, const char* name) {
  char buf[32];
  bool error = false;

  Serial.printf("Loading %s (%d bytes) from EEPROM...", name, sizeof(*t));
  memset(s, 0, sizeof(*s));
  if (!rtceeprom.eeprom_read(addr, (byte *) s, sizeof(*s))) {
    Serial.print(F("eeprom failed, ")); // fallback to default values?
    sprintf(buf, "load %s eeprom failed", name);
    logMsg(buf);
    error = true;
  }

  // check for valid CRC16, load defaults if check fails
  if (!error && crc16((uint8_t *) s, crclen) != s->crc) {
    if (s->crc) {
      Serial.print(F("crc error, "));
      sprintf(buf, "load %s crc error", name);
      logMsg(buf);
    }
    error = true;
  }
  
  if (error) {
    Serial.println(F("using defaults."));
    logMsg("loading default settings");
    return;
  }

  // overwrite settings with data stored in EEPROM
  memcpy((void*)t, s, sizeof(*t));
  Serial.println(F("OK."));
}


// template to save struct with settings to EEPROM
template <typename T>
bool saveSettings(T t, uint16_t addr, const char* name) {
  char buf[32];
  
  Serial.printf("Saving %s to EEPROM address 0x%04x...", name, addr);
  if (!rtceeprom.eeprom_write(addr, &t, sizeof(t))) {
    sprintf(buf, "save %s failed", name);
    logMsg(buf);
    Serial.println(F("failed!"));
    return false;
  } else {
    Serial.println(F("OK."));
    return true;
  }
}


// template to reset struct with settings in use and stored in EEPROM
template <typename T>
bool resetSettings(T t, uint16_t addr, const char* name) {
  char buf[32];
  
  Serial.printf("Reset %s...", name);
  if (!rtceeprom.eeprom_write(addr, &t, sizeof(t))) {
    sprintf(buf, "reset %s failed", name);
    logMsg(buf);
    Serial.println(F("failed!"));
    return false;
  } else {
    Serial.println(F("OK."));
    return true;
  }
}
#endif
