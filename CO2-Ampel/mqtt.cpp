/***************************************************************************
  Copyright (c) 2020 Lars Wessels

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
#include "config.h"

mqttprefs_t mqttSettings, mqttSettingsDefault;

static char clientname[32];
static bool mqttInited = false;

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

  if (co2status <= ALARM) {
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
    Serial.println(F("OK."));
    return true;
  } else {
    Serial.println(F("failed!"));
    logMsg("mqtt json failed");
    return false;
  }
}


// publish all sensor readings on seperate subtopics
static bool mqttSingle() {
  static char topicStr[128];
  char buf[128];
  char valueStr[8];
  uint8_t count = 0;

  Serial.printf("MQTT: publish readings to %s/%s...",
      mqttSettings.broker, mqttSettings.topic);

  if (co2status <= ALARM) {
    memset(topicStr, 0, sizeof(topicStr));
    itoa(scd30_co2ppm, valueStr, 10);
    sprintf(topicStr, "%s/co2median", mqttSettings.topic);
    if (mqtt.publish(topicStr, valueStr))
      count++;

    delay(MQTT_PUSH_DELAY_MS);
    memset(topicStr, 0, sizeof(topicStr));
    dtostrf(bme280_temperature, 5, 2, valueStr);
    sprintf(topicStr, "%s/temperature", mqttSettings.topic);
    if (mqtt.publish(topicStr, removeSpaces(valueStr)))
      count++;

    delay(MQTT_PUSH_DELAY_MS);
    memset(topicStr, 0, sizeof(topicStr));
    itoa(bme280_pressure, valueStr, 10);
    sprintf(topicStr, "%s/pressure", mqttSettings.topic);
    if (mqtt.publish(topicStr, removeSpaces(valueStr)))
      count++;

    if (hasBME280) {
      delay(MQTT_PUSH_DELAY_MS);
      memset(topicStr, 0, sizeof(topicStr));
      itoa(bme280_humidity, valueStr, 10);
      sprintf(topicStr, "%s/hum", mqttSettings.topic);
      if (mqtt.publish(topicStr, valueStr))
        count++;
    }
  }
  
  delay(MQTT_PUSH_DELAY_MS);
  memset(topicStr, 0, sizeof(topicStr));
  dtostrf(getVBAT(), 4, 2, valueStr);
  sprintf(topicStr, "%s/vbat", mqttSettings.topic);
  if (mqtt.publish(topicStr, removeSpaces(valueStr)))
    count++;

  delay(MQTT_PUSH_DELAY_MS); 
  memset(topicStr, 0, sizeof(topicStr));
  itoa(co2status, valueStr, 10);
  sprintf(topicStr, "%s/status",mqttSettings.topic);
  if (mqtt.publish(topicStr, statusNames[co2status]))
    count++;

  if (count == 6 || (count == 5 && !hasBME280) || (co2status > ALARM && count == 2)) {
    Serial.println(F("OK."));
    return true;
  } else {
    logMsg("mqtt single failed");
    Serial.println(F("failed!"));
    return false;
  }
}


void mqtt_init() {
  String name;

  if (mqttInited)
    return;

  if (!wifi_uplink(false)) {
    Serial.println(F("MQTT: failed to start, no WiFi uplink."));
    return;
  }
  
  if (mqttSettings.enabled && strlen(mqttSettings.broker) >= 4) {
    Serial.println(F("MQTT started."));
    mqtt.setServer(mqttSettings.broker, MQTT_PORT);
    name = String(MQTT_CLIENT_NAME).substring(0,24) + "-" + systemID();
    sprintf(clientname, name.c_str(), name.length());
    mqttInited = true;
  }
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
  
  setDefaults(&mqttSettings);  // set struct with defaults values from config.h
  memcpy((void*)&mqttSettingsDefault, &mqttSettings, sizeof(mqttSettings)); // keep a copy for reset
  loadSettings(&mqttSettings, &buf, offsetof(mqttprefs_t, crc), EEPROM_MQTT_SETTINGS_ADDR, "MQTT settings");
  printMQTTSettings(&mqttSettings);
}


bool saveMQTTSettings() {
  mqttSettings.crc = crc16((uint8_t *) &mqttSettings, offsetof(mqttprefs_t, crc));
  return saveSettings(mqttSettings, EEPROM_MQTT_SETTINGS_ADDR, "MQTT settings") && printMQTTSettings(&mqttSettings);
}


bool resetMQTTSettings() {
  memcpy((void*)&mqttSettings, &mqttSettingsDefault, sizeof(mqttSettings)); // use copy with defaults
  return resetSettings(mqttSettings, EEPROM_MQTT_SETTINGS_ADDR, "MQTT settings") && printMQTTSettings(&mqttSettings);

}
