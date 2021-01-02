/***************************************************************************
  Copyright (c) 2020-2021 Lars Wessels

  This file a part of the "CO2-Ampel" source code.
  https://github.com/lrswss/co2ampel

  Published under Apache License 2.0

***************************************************************************/

#include "mqtt.h"
#include "wifi.h"
#include "utils.h"
#include "sensors.h"
#include "logging.h"
#include "rtc.h"
#include "led.h"
#include "config.h"

mqttprefs_t mqttSettings;

static char clientname[64];
static bool mqttInited = false;
static uint16_t mqttMessageCount = 0;

WiFiClient wifi;
PubSubClient mqtt(wifi);


// load defaults settings from config.h
static void setDefaults(mqttprefs_t *s) {
  // an explicit memset is required to ensure zeroed padding
  // bytes in struct which would otherwise break CRC
  memset(s, 0, sizeof(*s));

  s->pushInterval = MQTT_PUSH_INTERVAL_SECS;
#ifdef ENABLE_MQTT
  s->enabled = true;
#endif
#if defined(MQTT_USER) && defined(MQTT_PASS)
  s->enableAuth = true;
#endif
#ifdef MQTT_PUSH_JSON
  s->enableJSON = true;
#endif
#if defined(MQTT_BROKER) && defined(MQTT_TOPIC)
  strncpy(s->broker, MQTT_BROKER, sizeof(mqttSettings.broker));
  strncpy(s->topic, MQTT_TOPIC, sizeof(mqttSettings.topic));
#endif
#if defined(MQTT_USER) && defined(MQTT_PASS)
  strncpy(s->username, MQTT_USER, sizeof(mqttSettings.username));
  strncpy(s->password, MQTT_PASS, sizeof(mqttSettings.password));
#endif
 s->crc = crc16((uint8_t *) s, offsetof(mqttprefs_t, crc));
}


static bool printMQTTSettings(mqttprefs_t *s) {
#ifdef SETTINGS_DEBUG
  Serial.printf("Enabled: %d\n", s->enabled);
  Serial.printf("Broker: %s\n", s->broker);
  Serial.printf("Topic: %s\n", s->topic);
  Serial.printf("Push interval: %d\n", s->pushInterval);
  Serial.printf("User: %s\n", s->username);
  Serial.printf("Password: %s\n", s->password);
  Serial.printf("EnableAuth: %d\n", s->enableAuth);
  Serial.printf("EnableJSON: %d\n", s->enableJSON);
  Serial.printf("CRC: %d\n", s->crc);
#endif  
  return true;
}


// send sensor data as JSON
static bool mqttJSON() {
  StaticJsonDocument<128> JSON;
  char buf[128];

  JSON["device"] = systemID();
  if (co2status > WARMUP && co2status <= ALARM) {
    JSON["co2median"] = scd30_co2ppm;
    JSON["temperature"] = bme280_temperature;
    if (hasBME280)
      JSON["humidity"] = bme280_humidity;
    JSON["pressure"] = bme280_pressure;
  }
  JSON["co2status"] = statusNames[co2status];
  JSON["vbat"] = ((int)(getVBAT()*100)) / 100.0;

  Serial.printf("MQTT: publish readings as JSON to %s/%s...",
    mqttSettings.broker, mqttSettings.topic);

  size_t s = serializeJson(JSON, buf);
  if (mqtt.publish(mqttSettings.topic, buf, s)) {
    blink_leds(SYSTEM_LED1, ORANGE, 100, 2, true);
    mqttMessageCount++;
    Serial.println(F("OK."));
    sprintf(buf, "mqtt json publish %d", mqttMessageCount);
    logMsg(buf);
    return true;
  } else {
    Serial.println(F("failed!"));
    blink_leds(SYSTEM_LEDS, RED, 250, 2, true);
    logMsg("mqtt json failed");
    return false;
  }
}


// publish all sensor readings on seperate subtopics
static bool mqttSingle() {
  static char topicStr[96], buf[128];
  char valueStr[8];
  uint8_t count = 0;

  Serial.printf("MQTT: publish readings to %s/%s/%s...",
      mqttSettings.broker, mqttSettings.topic, systemID().c_str());

  if (co2status > WARMUP && co2status <= ALARM) {
    memset(topicStr, 0, sizeof(topicStr));
    itoa(scd30_co2ppm, valueStr, 10);
    sprintf(topicStr, "%s/%s/co2median", mqttSettings.topic, systemID().c_str());
    if (mqtt.publish(topicStr, valueStr))
      count++;

    delay(MQTT_PUSH_DELAY_MS);
    memset(topicStr, 0, sizeof(topicStr));
    dtostrf(bme280_temperature, 5, 2, valueStr);
    sprintf(topicStr, "%s/%s/temperature", mqttSettings.topic, systemID().c_str());
    if (mqtt.publish(topicStr, removeSpaces(valueStr)))
      count++;

    delay(MQTT_PUSH_DELAY_MS);
    memset(topicStr, 0, sizeof(topicStr));
    itoa(bme280_pressure, valueStr, 10);
    sprintf(topicStr, "%s/%s/pressure", mqttSettings.topic, systemID().c_str());
    if (mqtt.publish(topicStr, removeSpaces(valueStr)))
      count++;

    if (hasBME280) {
      delay(MQTT_PUSH_DELAY_MS);
      memset(topicStr, 0, sizeof(topicStr));
      itoa(bme280_humidity, valueStr, 10);
      sprintf(topicStr, "%s/%s/hum", mqttSettings.topic, systemID().c_str());
      if (mqtt.publish(topicStr, valueStr))
        count++;
    }
  }
  
  delay(MQTT_PUSH_DELAY_MS);
  memset(topicStr, 0, sizeof(topicStr));
  dtostrf(getVBAT(), 4, 2, valueStr);
  sprintf(topicStr, "%s/%s/vbat", mqttSettings.topic, systemID().c_str());
  if (mqtt.publish(topicStr, removeSpaces(valueStr)))
    count++;

  delay(MQTT_PUSH_DELAY_MS); 
  memset(topicStr, 0, sizeof(topicStr));
  itoa(co2status, valueStr, 10);
  sprintf(topicStr, "%s/%s/status", mqttSettings.topic, systemID().c_str());
  if (mqtt.publish(topicStr, statusNames[co2status]))
    count++;

  if (count == 6 || (count == 5 && !hasBME280) ||
    ((co2status > ALARM  || co2status <= WARMUP ) && count == 2)) {
    blink_leds(SYSTEM_LED1, ORANGE, 100, 2, true);
    mqttMessageCount++;
    sprintf(buf, "mqtt single publish %d", mqttMessageCount);
    logMsg(buf);
    Serial.println(F("OK."));
    return true;
  } else {
    blink_leds(SYSTEM_LEDS, RED, 250, 2, true);
    logMsg("mqtt single failed");
    Serial.println(F("failed!"));
    return false;
  }
}


bool mqtt_init() {
  String name;
  char buf[96];

  if (mqttInited)
    return true;

  if (!wifi_uplink(false)) {
    Serial.println(F("MQTT: failed to start, no WiFi uplink."));
    return false;
  }
  
  if (mqttSettings.enabled && strlen(mqttSettings.broker) >= 4) {
    Serial.println(F("MQTT started."));
    sprintf(buf, "mqtt started, %s/%s", mqttSettings.broker, mqttSettings.topic);
    logMsg(buf);
    mqtt.setServer(mqttSettings.broker, MQTT_PORT);
    name = String(MQTT_CLIENT_NAME).substring(0,48) + "-" + systemID() + "-" + String(random(0xffff), HEX);
    sprintf(clientname, name.c_str(), name.length());
    blink_leds(SYSTEM_LEDS, ORANGE, 100, 2, true);
    mqttInited = true;
  }
  delay(1000);
  return mqttInited;
}


void mqtt_stop() {
  if (!mqttInited)
    return;
  Serial.println(F("MQTT stopped."));
  mqttInited = false;
  mqtt.disconnect(); 
}


// try to publish sensor reedings with given timeout either 
// as a single json strings or each reading on its own topic
// will implicitly call mqtt_init()
bool mqtt_send(uint16_t timeoutMillis) {
  uint8_t retries = 0;

  save_leds();
  if (!mqttInited)
    mqtt_init();

  if (!wifi_uplink(false)) {
    Serial.println(F("MQTT: cannot send, no WiFi uplink."));
    return false;
  }
  
  while (retries++ < int(timeoutMillis/100)) {
    if (mqtt.connected()) {
      if (mqttSettings.enableJSON)
        return mqttJSON();
      else
        return mqttSingle();
    }
    if (mqttSettings.enableAuth)
      mqtt.connect(clientname, mqttSettings.username, mqttSettings.password);
    else
      mqtt.connect(clientname);
    delay(100);
  }
  Serial.println(F("MQTT: failed to connect to broker."));
  logMsg("mqtt failed (connect error)");
  return false;
}


// read general device settings from DS3231 EEPROM
void loadMQTTSettings() {
  mqttprefs_t buf;

  memset(&mqttSettings, 0, sizeof(mqttSettings));
  setDefaults(&mqttSettings); // set struct with defaults values from config.h
  loadSettings(&mqttSettings, &buf, offsetof(mqttprefs_t, crc), EEPROM_MQTT_SETTINGS_ADDR, "MQTT settings");
  printMQTTSettings(&mqttSettings);
}


// returns number of successful MQTT push messages
uint16_t mqtt_messages() {
  return mqttMessageCount;
}


bool saveMQTTSettings() {
  if (mqttSettings.crc) {
    mqttSettings.crc = crc16((uint8_t *) &mqttSettings, offsetof(mqttprefs_t, crc));
    return saveSettings(mqttSettings, EEPROM_MQTT_SETTINGS_ADDR, "MQTT settings") && printMQTTSettings(&mqttSettings);
  }
  return false;
}


bool resetMQTTSettings() {
  Serial.println(F("Reset MQTT settings."));
  logMsg("reset MQTT settings");
  memset(&mqttSettings, 0, sizeof(mqttSettings));
  setDefaults(&mqttSettings);
  return saveSettings(mqttSettings, EEPROM_MQTT_SETTINGS_ADDR, "MQTT settings") && printMQTTSettings(&mqttSettings);
}
