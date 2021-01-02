/***************************************************************************
  Copyright (c) 2020-2021 Lars Wessels

  This file a part of the "CO2-Ampel" source code.
  https://github.com/lrswss/co2ampel

  Published under Apache License 2.0

***************************************************************************/

#ifndef _LORAWAN_H
#define _LORAWAN_H

#include <Arduino.h>
#include "config.h"
#ifdef HAS_LORAWAN_SHIELD
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

// DS3231 EEPROM 24C32
#define EEPROM_LORAWAN_SETTINGS_ADDR 0x300
#define EEPROM_LORAWAN_SESSION_ADDR 0x400

#define LORAWAN_TXPOWER 14
#define LORAWAN_NETID_TTN 0x13

#ifndef LORAWAN_DRTX
#define LORAWAN_DRTX DR_SF8
#endif
#ifndef LORAWAN_DRJOIN
#define LORAWAN_DRJOIN DR_SF12
#endif

#if LORAWAN_DRJOIN > LORAWAN_DRTX
#define LORAWAN_DRJOIN LORAWAN_DRTX
#endif

#ifndef LORAWAN_DEVEUI
#error "LORAWAN_DEVEUI needs to be preset in config.h"
#endif

#if !defined(LORAWAN_USETTN) && !defined(LORAWAN_NETID)
#error "LORAWAN_NETID needs to be preset in config.h since LORAWAN_USETTN is not set"
#elif defined(LORAWAN_USETTN)
#define LORAWAN_NETID LORAWAN_NETID_TTN
#endif

typedef struct {
  lmic_t lmic;
  uint16_t crc;
} session_t;

typedef struct {
  bool enabled;
  uint16_t txInterval;
  uint8_t drJoin;
  uint8_t drSend;
  bool useTTN;
  bool useOTAA;
  uint32_t netid;
  uint32_t devAddr;
  byte nwksKey[16];
  byte appsKey[16];
  byte appEui[8];
  byte appKey[16];
  uint16_t crc = 0;
} loraprefs_t;

extern osjob_t lorawanjob;
extern session_t lorawanSession;
extern loraprefs_t lorawanSettings;

void lmic_init();
void lmic_stop();
void lmic_send(osjob_t* job);
bool lmic_ready();
void waitForLorawanJobs(uint8_t secs, bool toogleLED);
void loadLoRaWANSession();
bool saveLoRaWANSession();
bool resetLoRaWANSession();
void loadLoRaWANSettings();
bool saveLoRaWANSettings(bool updatesession);
bool resetLoRaWANSettings();
#endif
#endif
