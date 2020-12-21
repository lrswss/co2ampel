/***************************************************************************
  Copyright (c) 2020 Lars Wessels

  This file a part of the "CO2-Ampel" source code.
  https://github.com/lrswss/co2ampel

  Published under Apache License 2.0

***************************************************************************/

#include "rtc.h"
#include "led.h"
#include "logging.h"
#include "utils.h"

RTC_DS3231 rtc;

// setup the ntp udp client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, 0);

// TimeZone Settings Details https://github.com/JChristensen/Timezone
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};
Timezone CE(CEST, CET);  // Frankfurt, Paris

bool rtcOK = false;
sysprefs_t settings, settingsDefault;
uEEPROMLib rtceeprom(0x57);


// preset settings struct with defaults from config.h
static void setDefault(sysprefs_t *s) {
  // an explicit memset is required to ensure zeroed padding
  // bytes in struct which would otherwise break CRC
  memset(s, 0, sizeof(*s));

  s->scd30TempOffset = SCD30_TEMP_OFFSET;
  s->co2MediumThreshold = CO2_MEDIUM_THRESHOLD;
  s->co2HighThreshold = CO2_HIGH_THRESHOLD;
  s->co2AlarmThreshold = CO2_ALARM_THRESHOLD;
  s->co2ThresholdHysteresis = CO2_THRESHOLD_HYSTERESIS;
  s->co2ReadingInterval = SCD30_INTERVAL_SECS;
  s->co2MedianSamples = SCD30_NUM_SAMPLES_MEDIAN;
  s->beginSleep = BEGIN_SLEEP_HOUR;
  s->endSleep = END_SLEEP_HOUR;
  s->loggingInterval = LOGGING_INTERVAL_SECS;
  s->altitude = ALTITUDE_ABOVE_SEELEVEL;
  s->medianFilter = true;
#ifdef ENABLE_NOOP
  s->enableNOOP = true;
#endif
#ifdef ENABLE_AUTH
  s->enableAuth = true;
#endif
#ifdef ENABLE_LOGGING
  s->enableLogging = true;
#endif
#if defined(SETTINGS_USERNAME) && defined(SETTINGS_PASSWORD)
  strncpy(s->authUsername, SETTINGS_USERNAME, sizeof(s->authUsername));
  strncpy(s->authPassword, SETTINGS_PASSWORD, sizeof(s->authPassword));
#endif
}


static bool printGeneralSettings(sysprefs_t *s) {
#ifdef SETTINGS_DEBUG
  Serial.printf("scd30TempOffset: %d\n", s->scd30TempOffset);
  Serial.printf("co2MediumThreshold: %d\n", s->co2MediumThreshold);
  Serial.printf("co2HighThreshold: %d\n", s->co2HighThreshold);
  Serial.printf("co2AlarmThreshold: %d\n", s->co2AlarmThreshold);
  Serial.printf("co2ThresholdHysteresis: %d\n", s->co2ThresholdHysteresis);
  Serial.printf("co2ReadingInterval: %d\n", s->co2ReadingInterval);
  Serial.printf("co2MedianSamples: %d\n", s->co2MedianSamples);
  Serial.printf("medianFilter: %d\n", s->medianFilter);
  Serial.printf("enableNOOP: %d\n", s->enableNOOP);
  Serial.printf("beginSleep: %d\n", s->beginSleep);
  Serial.printf("endSleep: %d\n", s->endSleep);
  Serial.printf("enableAuth: %d\n", s->enableAuth);
  Serial.printf("authUsername: %s\n", s->authUsername);
  Serial.printf("authPassword: %s\n", s->authPassword);
  Serial.printf("enableLogging: %d\n", s->enableLogging);
  Serial.printf("loggingInterval: %d\n", s->loggingInterval);
  Serial.printf("altitude: %d\n", s->altitude);
  Serial.printf("CRC: %d\n", s->crc);
#endif
  return true;
}


void rtc_init() {
  if (!rtc.begin()) {
    Serial.println(F("Couldn't find RTC...system halted."));
    logMsg("rtc error");
    while (1) {
      blink_leds(HALF_RING, RED, 500, 2, false);
      delay(2000);
    }
  }
  rtc.disable32K();
  rtc_temperature();
  blink_leds(SYSTEM_LEDS, GREEN, 100, 2, true);
  rtcOK = true;
  delay(1000);
}


void rtc_temperature() {
  Serial.print(F("DS3231: temperature("));
  Serial.print(rtc.getTemperature(), 1);
  Serial.println("C)");
}


// returns ptr to array with current time
char* getTimeString(bool showsecs) {
  static char strTime[9]; // HH:MM:SS
  uint32_t epochTime = 0;
  DateTime now;
  time_t t;

  if (rtcOK) {
    now = rtc.now();
    epochTime = now.unixtime();
  }  
  t = CE.toLocal(epochTime);

  memset(strTime, 0, sizeof(strTime));
  if (epochTime > 1605441600) {  // 15.11.2020
    if (showsecs)
      sprintf(strTime, "%.2d:%.2d:%.2d", hour(t), minute(t), second(t));
    else
      sprintf(strTime, "%.2d:%.2d", hour(t), minute(t));
  } else {
    if (showsecs)
      strncpy(strTime, "--:--:--", 8);
    else
      strncpy(strTime, "--:--", 5);
  }
  return strTime;
}


// returns ptr to array with current date
char* getDateString() {
  static char strDate[11]; // DD.MM.YYYY
  uint32_t epochTime = 0;
  DateTime now;
  time_t t;

  if (rtcOK) {
    now = rtc.now();
    epochTime = now.unixtime();
  }  
  t = CE.toLocal(epochTime);

  if (epochTime > 1605441600) {
    sprintf(strDate, "%.2d.%.2d.%4d", day(t), month(t), year(t));
  } else {
    strncpy(strDate, "--.--.----", 10); 
  }
  return strDate;
}


// return current hour (local time)
int8_t getHourLocal() {
  if (rtcOK) {
    return hour(CE.toLocal(rtc.now().unixtime()));
  } else {
    return -1;
  }
}


// start ntp client and update RTC
bool startNTPSync() {  
  Serial.print(F("Syncing RTC with NTP-Server..."));
  timeClient.begin();
  timeClient.forceUpdate(); // takes a while
  if (timeClient.getEpochTime() > 1000) {
    Serial.println(F("OK."));
    rtc.adjust(DateTime(timeClient.getEpochTime()));
    logMsg("ntp sync");
    delay(1000);
    return true;
  } else {
    Serial.println(F("failed!"));
    return false;
  }
}


void stopNTPSync() {
  timeClient.end();
}


// read general device settings from DS3231 EEPROM
void loadGeneralSettings() {
  sysprefs_t buf;
  
  setDefault(&settings);  // set struct with defaults values from config.h
  memcpy((void*)&settingsDefault, &settings, sizeof(settings)); // keep a copy for reset
  loadSettings(&settings, &buf, offsetof(sysprefs_t, crc), EEPROM_SYSTEM_PREFS_ADDR, "general settings");
  printGeneralSettings(&settings);
}


// store general settings in DS3231 EEPROM
bool saveGeneralSettings() {
  settings.crc = crc16((uint8_t *) &settings, offsetof(sysprefs_t, crc));
  return saveSettings(settings, EEPROM_SYSTEM_PREFS_ADDR, "general settings") && printGeneralSettings(&settings);
}


// reset general settings to default values and store them to EEPROM
bool resetGeneralSettings() {
  memcpy((void*)&settings, &settingsDefault, sizeof(settings)); // use copy with defaults
  return resetSettings(settings, EEPROM_SYSTEM_PREFS_ADDR, "general settings") && printGeneralSettings(&settings);
}
