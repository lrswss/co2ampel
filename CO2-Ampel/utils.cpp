/***************************************************************************
  Copyright (c) 2020 Lars Wessels

  This file a part of the "CO2-Ampel" source code.
  https://github.com/lrswss/co2ampel

  Published under Apache License 2.0

***************************************************************************/

#include "config.h"
#include "utils.h"
#include "lorawan.h"
#include "led.h"
#include "logging.h"
#include "rtc.h"
#include "wifi.h"
#include "mqtt.h"

RunningMedian vbat_readings = RunningMedian(10);

rstcodes runmode;
char runmodes[7][10] = {
  "reset",
  "restart",
  "powerup",
  "wakeup",
  "exception",
  "watchdog",
  "other"
};


// returns the current battery voltage
static float readBatteryVoltage() {
  uint16_t raw = 0;
  float vbat;

  analogRead(A0);
  for (uint8_t i = 0; i < 10; i++) {
    raw += analogRead(A0);
    delay(1);
  }
  vbat = (raw/10.0*VBAT_ADJUST)/1024.0;
  if (vbat >= 2.0)
    return vbat;
  else
    return 0.0;
}


// returns running median value
float getVBAT() {  
  float vbat;

  vbat = readBatteryVoltage();
  vbat_readings.add(vbat);
  
  // fill running median array upon startup
  while (vbat_readings.getCount() < vbat_readings.getSize())
    vbat_readings.add(vbat);
    
  return vbat_readings.getMedian();
}


// check battery voltage
void checkLowBat() {
  static char buf[32], vbatStr[6];
  float vbat;

  vbat = getVBAT();
  if (vbat <= 2.0) {
    Serial.println(F("Battery not connected."));
  } else if (vbat <= VBAT_DEEPSLEEP) {
    blink_leds(QUARTER_RING, RED, 100, 4, false);
    dtostrf(vbat, 4, 2, vbatStr);
    Serial.printf("WARNING: low battery %sV, enter deep sleep!", vbatStr);
    sprintf(buf, "low battery %sV, sleeping", vbatStr);
    logMsg(buf);
    enterDeepSleep(3600);
  } else {
    Serial.print(F("Battery voltage: "));
    Serial.println(vbat);
  }
}


void enterDeepSleep(uint32_t secs) {
  if (secs > 4200) // max. 71 minutes
      secs = 4200;
      
  saveGeneralSettings();
  saveWifiSettings();
  saveMQTTSettings();
#ifdef HAS_LORAWAN_SHIELD
  if (lorawanSettings.enabled && lmic_ready())
    lmic_stop();
#endif
  scd30_sleep();
  Serial.printf("Sleeping for %d secs...\n", secs);
  clear_leds(ALL_LEDS);
  delay(1000);
  blink_leds(HALF_RING, BLUE, 250, 2, false);
  Serial.flush();
  system_deep_sleep_set_option(2); // avoid RF recalibration after wake up
  ESP.deepSleep(secs*1000000);
}


void resetSystem() {
  clear_leds(ALL_LEDS);
  Serial.println(F("Restarting system..."));
  logMsg("reset");
  delay(1000);
  blink_leds(HALF_RING, RED, 100, 2, false);
  Serial.flush();
  ESP.restart();
}


bool checkNOOPTime(uint8_t begin_hour, uint8_t end_hour) {
  static char buf[64];
  bool noop_time = false;
  uint32_t epochTime;
  DateTime now;
  time_t t;

  if (!settings.enableNOOP)
      return false;

  if (rtcOK) {
    now = rtc.now();
    epochTime = now.unixtime(); 
  }
  t = CE.toLocal(epochTime);
  
  // either disabled or abort due to unvalid RTC time
  if (begin_hour == end_hour || year(t) < 2020)
    return false;
  // check for valid configuration settings
  if (begin_hour > 23 || begin_hour < 0 || end_hour > 23 || end_hour < 0)
    return false;

  if (begin_hour < end_hour) {
    if (begin_hour <= hour(t) && hour(t) < end_hour)
      noop_time = true;
  } else if (begin_hour <= hour(t) || hour(t) < end_hour) {
    noop_time = true;
  }

  if (noop_time) {
    co2status = NOOP;
    Serial.printf("NOOP from %.2d:00 to %.2d:00\n", begin_hour, end_hour);
    sprintf(buf, "noop %.2d:00-%.2d:00", begin_hour, end_hour);
    logMsg(buf);
  }

  return noop_time;
}


// start and scan I2C for devices
// return number of devices found
uint8_t i2c_init() {
  uint8_t addr, error, devices;
  
  Wire.begin();
  Wire.setClock(100000); // see SCD30_Interface_Description.pdf (May 2020)
  Wire.setClockStretchLimit(30000); // 30ms
  devices = 0;
  Serial.print(F("Scanning I2C bus..."));
  for (addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    error = Wire.endTransmission();
    if (error == 0) {
      devices++;
    } else if (error == 4) {
      Serial.print("unknown error for address 0x");
      if (addr < 16) 
        Serial.print("0");
      Serial.println(addr, HEX);
      return 0;
    }
  }
  Serial.printf("%d devices found.\n", devices);
  return devices;
}


// returns given string without spaces
char* removeSpaces(char *str) {
  uint8_t i = 0, j = 0;
  while (str[i++]) {
    if (str[i-1] != ' ')
      str[j++] = str[i-1];
  }
  str[j] = '\0';
  return str;
}


// checksum for EEPROM
uint16_t crc16(const uint8_t *data, uint8_t len) {
  uint8_t x;
  uint16_t crc = 0xFFFF;

  while (len--) {
    x = (crc >> 8) ^ *data++;
    x ^= x >> 4;
    crc = (crc << 8) ^ (x << 12) ^ (x << 5) ^ x;
  }
  crc &= 0xffff;
  return crc;
}


// returns hardware system id (last 3 bytes of mac address)
String systemID() {
  uint8_t mac[6];
  char sysid[7];
  
  WiFi.macAddress(mac);
  sprintf(sysid, "%02X%02X%02X", mac[3], mac[4], mac[5]);
  return String(sysid);
}


void bootMessage() {
  struct rst_info *rstInfo;

  Serial.print(F("ESP Core Version: "));
  Serial.println(ESP.getCoreVersion());

  rstInfo = ESP.getResetInfoPtr();
  if (rstInfo->reason == REASON_DEEP_SLEEP_AWAKE) {
    Serial.println(F("Wake up from deep sleep."));
    runmode = WAKEUP;
  } else if (rstInfo->reason == REASON_SOFT_RESTART ) {
    Serial.println(F("System was restarted."));
    runmode = RESTART;
  } else if (rstInfo->reason == REASON_EXT_SYS_RST ) {
    Serial.println(F("Reset button was pressed."));
    runmode = RESET;
  } else if (rstInfo->reason == REASON_DEFAULT_RST) {
    Serial.println(F("Normal system power up."));
    runmode = POWERUP;
  } else if (rstInfo->reason == REASON_EXCEPTION_RST) {
    Serial.println(F("Exception caused reset."));
    runmode = EXCEPTION;    
  } else if (rstInfo->reason == REASON_SOFT_WDT_RST || rstInfo->reason == REASON_WDT_RST) {
    Serial.println(F("Watchdog was triggered."));
    runmode = WATCHDOG;    
  } else {
    Serial.print(F("Restart reason "));
    Serial.println(rstInfo->reason);
    runmode = OTHER;
  }
  Serial.println();
  delay(1000);
}


char* getRuntime(uint32_t runtimeSecs) {
  static char str[11];

  int days = runtimeSecs / 86400 ;
  int hours = (runtimeSecs % 86400) / 3600;
  int minutes = ((runtimeSecs % 86400) % 3600) / 60;
  int secs = (((runtimeSecs % 86400) % 3600) % 60);
  sprintf(str, "%dd %.2dh %.2dm %.2ds", days, hours, minutes, secs);

  return str;
}


char* lowercase(char* s) {
  for (char *p=s; *p; p++) *p=tolower(*p);
  return s;
}


#ifdef HAS_LORAWAN_SHIELD
// turn hex byte array of given length into a null-terminated hex string
void array2string(const byte *arr, int len, char *buf, bool reverse) {
  for (int i = 0; i < len; i++)
    sprintf(buf + i * 2, "%02X", reverse ? arr[len-1-i]: arr[i]);
}


// turn a hex string into a byte array of given length
void string2array(const char *buf, int len, byte *arr, bool reverse) {
  memset(arr, 0, len);
  for (int i = 0; i < len; i++) {
    byte nib1 = (*buf <= '9') ? *buf++ - '0' : *buf++ - '7';
    byte nib2 = (*buf <= '9') ? *buf++ - '0' : *buf++ - '7';
    if (reverse)
      arr[len-1-i] = (nib1 << 4) | (nib2 & 0x0F);
    else
      arr[i] = (nib1 << 4) | (nib2 & 0x0F);
  }
}


// convert null-terminated string of hex chars into a uint32_t
uint32_t hex2num(const char *buf) {
  uint32_t num = 0;
  while (*buf) {
    num <<= 4; // shift previous nibble one order up
    num +=  (*buf <= '9') ? *buf++ - '0' : *buf++ -'7'; // add nibble
  }
  return num;
}


// print byte array as hex string, optionally masking last 4 bytes
void printHEX8bit(uint8_t *arr, uint8_t len, bool ln, bool reverse, bool mask) {
  char hex[len * 2 + 1];
  array2string(arr, mask ? len-8 : len, hex, reverse);
  if (mask)
    strcat(hex, "XXXXXXXX");
  Serial.print(hex);
  if (ln)
    Serial.println();
}


// returns sum of all array elements
uint16_t array2int(uint8_t *arr, uint8_t len) {
  uint16_t sum = 0;
  for (uint8_t i = 0; i < len; i++) {
    sum += arr[i];
  }
  return sum;
}
#endif
