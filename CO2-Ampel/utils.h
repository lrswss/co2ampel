/***************************************************************************
  Copyright (c) 2020 Lars Wessels

  This file a part of the "CO2-Ampel" source code.
  https://github.com/lrswss/co2ampel

  Published under Apache License 2.0

***************************************************************************/

#ifndef _UTILS_H
#define _UTILS_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <RunningMedian.h>

extern "C" {
#include "user_interface.h"
}

// if battery voltage falls below this level
// resort to deep sleep to protect batteries
#define VBAT_DEEPSLEEP 3.55

enum rstcodes {
  RESET,
  RESTART,
  POWERUP,
  WAKEUP,
  EXCEPTION,
  WATCHDOG,
  OTHER
};

extern rstcodes runmode;
extern char runmodes[7][10];

void bootMessage();
uint8_t i2c_init();
void checkLowBat();
float getVBAT();
bool checkNOOPTime(uint8_t begin_hour, uint8_t end_hour);
char* getRuntime(uint32_t runtimeSecs);
uint16_t crc16(const uint8_t *data, uint8_t len);
char* removeSpaces(char *str);
String systemID();
void enterDeepSleep(uint32_t secs);
void resetSystem();
char* lowercase(char* s);
#ifdef HAS_LORAWAN_SHIELD
void array2string(const byte *arr, int len, char *buf, bool reverse);
void string2array(const char *buf, int len, byte *arr, bool reverse);
uint32_t hex2num(const char *buf);
void printHEX8bit(uint8_t *arr, uint8_t len, bool ln, bool reverse, bool mask);
uint16_t array2int(uint8_t *arr, uint8_t len);
#endif
#endif
