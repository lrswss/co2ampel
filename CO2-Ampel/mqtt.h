/***************************************************************************
  Copyright (c) 2020-2021 Lars Wessels

  This file a part of the "CO2-Ampel" source code.
  https://github.com/lrswss/co2ampel

  Published under Apache License 2.0

***************************************************************************/

#ifndef _MQTT_H
#define _MQTT_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

#define EEPROM_MQTT_SETTINGS_ADDR 0x200
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#define MQTT_CLIENT_NAME "co2ampel"
#define MQTT_PUSH_DELAY_MS 50
#ifndef MQTT_PUSH_INTERVAL_SECS
#define MQTT_PUSH_INTERVAL_SECS 60
#endif

typedef struct {
  bool enabled;
  uint16_t pushInterval;
  char broker[64];
  char topic[64];
  char username[32];
  char password[32];
  bool enableAuth;
  bool enableJSON;
  uint16_t crc = 0;
} mqttprefs_t;

extern mqttprefs_t mqttSettings;

bool mqtt_init();
void mqtt_stop();
bool mqtt_send(uint16_t timeoutMillis);
uint16_t mqtt_messages();
void loadMQTTSettings();
bool saveMQTTSettings();
bool resetMQTTSettings();
#endif
