/*******************************************************************************

  Copyright (c) 2020-2021 Lars Wessels

  Yet another "CO2-Ampel" based on an ESP8266 (Wemos D1 mini) to continuously
  measure CO2 concentration indoor with a SCD30 sensor accompanied by a BME280
  for temperature, humidity and air pressure readings. Current air condition
  (good, medium, critical, bad) is shown using a NeoPixel ring with WS2812
  LEDs illuminating the device in either green, yellow or red.

  Unlike other devices this one runs on two 18650 LiIon-batteries, offers a
  sophisticated web interface for configuration, live sensor readings and
  OTA firmware updates. Sensor data can be logged to local flash (LittleFS),
  retrieved RESTful, published using MQTT or transmitted with LoRaWAN if a
  RFM95 shield is installed.


  Licensed under the Apache License, Version 2.0 (the "License"); you may 
  not use this file except in compliance with the License. You may obtain 
  a copy of the License at http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT 
  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the 
  License for the specific language governing permissions and limitations 
  under the License.

 *******************************************************************************/

#include "config.h"
#include "led.h"
#include "rtc.h"
#include "sensors.h"
#include "webserver.h"
#include "wifi.h"
#include "logging.h"
#include "lorawan.h"
#include "mqtt.h"
#include "utils.h"


void setup() {
  Serial.begin(115200);
  delay(500);
  char buf[32];

  Serial.println();
  Serial.printf("%s (v%d)\n", "CO2-Ampel-BME280-LoRaWAN-MQTT-RESTful", FIRMWARE_VERSION);

  bootMessage();
  led_init();
  mountFS();

  sprintf(buf, "%s,v%d", runmodes[runmode], FIRMWARE_VERSION);
  logMsg(buf);
  
  pinMode(A0, INPUT);
  checkLowBat();
   
  // initialize I2C bus and check for (right) number of devices
  if (i2c_init() != 4) { // SCD30, BME280, DS3231, EEPROM
    Serial.println(F("I2C bus error...system halted!"));
    while (1) {
      logMsg("i2c error");
      blink_leds(ALL_LEDS, RED, 500, 1, false);
      delay(2000);
    }
  } else {
    blink_leds(HALF_RING, WHITE, 500, 2, false);
    delay(1000);
  }

  // reset button will clear system settings
  if (runmode == RESET) {
    logMsg("reset button");
    resetGeneralSettings();
    resetMQTTSettings();
    resetWifiSettings();
  } else {
    // load general settings from DS3231 EEPROM (24C32)
    loadGeneralSettings();
  }
  
  // check for power saving NOOP mode (requires valid RTC time)
  // in NOOP mode only webserver and MQTT are enabled (optional)
  checkNOOPTime(settings.beginSleep, settings.endSleep);

  // recommended: https://github.com/hallard/WeMos-Lora
#ifdef HAS_LORAWAN_SHIELD
  if (runmode == RESET) {
    resetLoRaWANSettings();
    resetLoRaWANSession();
  } else {
    loadLoRaWANSettings();
  }
  if (lorawanSettings.enabled && co2status != NOOP) {
    lmic_init(); // might trigger a (blocking) join request
  } else if (!lorawanSettings.enabled) {
    Serial.println(F("LoRaWAN disabled."));
    logMsg("lorawan disabled");
  }
#endif

  // always start an AP and fire up local webserver with (optional) timeout
  // to allow access to system settings or we'd be locked out until NOOP
  // time has finished...
  loadWifiSettings();
  wifi_hotspot(false);
  webserver_start((co2status == NOOP && runmode != RESET) ? WEBSERVER_TIMEOUT_NOOP : wifiSettings.webserverTimeout);

  // try to connect to local WiFi to update RTC and send push MQTT messages
  if (wifiSettings.enableWLANUplink) {
    wifi_uplink(true);
    startNTPSync();
  }
  Serial.printf("Local time (RTC): %s, %s\n", getDateString(), getTimeString(true));
  
  loadMQTTSettings();
  if (wifiSettings.enableWLANUplink && mqttSettings.enabled) {
    mqtt_send(500); // send initial alive message after system startup
  } else if (!mqttSettings.enabled) {
    Serial.println(F("MQTT disabled."));
    logMsg("mqtt disabled");
  } else {
    Serial.println(F("MQTT: no WiFi uplink, disabled."));
    logMsg("mqtt disabled (no wifi)");
  }

  if (co2status != NOOP) {
    sensors_init();
    rotateLogs();
    Serial.println(F("Setup completed.\n"));
  } else {
    // sensors are disabled in NOOP mode
    Serial.print(F("System will power down in "));
    if (runmode != RESET)
      Serial.print(WEBSERVER_TIMEOUT_NOOP);
    else
      Serial.print(wifiSettings.webserverTimeout);
    Serial.println(F(" secs...\n"));
  }
}


void loop() {
  static uint32_t failureStateSecs, failureCountdown, runtimeCounterSecs = 1;
  static uint32_t prevSecond, prevReadings = millis()/1000;

  // check for webserver timeout and handle browser requests
  if (!webserver_stop(false))
    webserver.handleClient();

  // initial warmup for scd30 co2 sensor
  if (millis()/1000 <= SCD30_WARMUP_SECS) {
    if (co2status == NODATA) {
      Serial.print(F("System warm up, starting periodic sensor readings in "));
      Serial.print(SCD30_WARMUP_SECS - millis()/1000);
      Serial.println(F(" seconds..."));
      co2status = WARMUP;
      set_leds(HALF_RING, WHITE);
      scd30_warmup_countdown = SCD30_WARMUP_SECS - int(millis()/1000);
      logReadings(runtimeCounterSecs);
    }

  // do not query sensors in state CALIBRATE, FAILURE or NOOP
  } else if (co2status < CALIBRATE && 
      (millis()/1000 - prevReadings >= max(5, int(settings.co2ReadingInterval)))) { // lower limit 5 sec.
    prevReadings = millis()/1000;
    //rtc_temperature();
    bme280_readings(true);
    if (scd30_readings(false)) {
      if (scd30_co2ppm < CO2_LOWER_BOUND) {
        if (co2status != NODATA) {
          co2status = NODATA;
          logReadings(runtimeCounterSecs); // don't switch color, just log intermediate failed readings
        }   
      } else if ((co2status <= GOOD && scd30_co2ppm <= settings.co2MediumThreshold) ||
          (co2status == MEDIUM && (scd30_co2ppm + CO2_THRESHOLD_HYSTERESIS) <= settings.co2MediumThreshold)) {
        if (co2status != GOOD) {
          set_leds(QUARTER_RING, GREEN);
          co2status = GOOD;
          logReadings(runtimeCounterSecs);
        }
      } else if ((co2status <= MEDIUM && scd30_co2ppm <= settings.co2HighThreshold) ||
          (co2status == CRITICAL && (scd30_co2ppm + CO2_THRESHOLD_HYSTERESIS) <= settings.co2HighThreshold)) {
        if (co2status != MEDIUM) {
          set_leds(HALF_RING, YELLOW);
          co2status = MEDIUM;
          logReadings(runtimeCounterSecs);
        }
      } else if ((co2status <= CRITICAL && scd30_co2ppm <= settings.co2AlarmThreshold) ||
          (co2status == ALARM && (scd30_co2ppm + CO2_THRESHOLD_HYSTERESIS) <= settings.co2AlarmThreshold)) {
        if (co2status != CRITICAL) {
          set_leds(HALF_RING, RED);
          co2status = CRITICAL;
          logReadings(runtimeCounterSecs);
        }
      } else if (co2status != ALARM) {
          co2status = ALARM;
          logReadings(runtimeCounterSecs);

      }
      Serial.printf("Condition: %s\n", statusNames[co2status]);

    } else if (co2status != FAILURE) {
        co2status = FAILURE;
        Serial.println(F("Switching to state FAILURE!"));
        logMsg("system failure");
        clear_leds(FULL_RING);
        logReadings(runtimeCounterSecs);
    }
  }

  // main control loop for periodic actions
  if (millis() - prevSecond >= 1000) { // once every seconds
    prevSecond = millis();

    // stop local AP if webserver has stopped
    if (webserver_stop(false))
      wifi_hotspot(true);

    // switch off wifi after webserver timeout unless MQTT push is enabled
    if (!mqttSettings.enabled && webserver_stop(false) && wifi_uplink(false)) {
      stopNTPSync();
      wifi_stop();
    }

    if (co2status == NOOP && !settings.enableNOOP) {
      // if currently active NOOP mode has just been disabled in webui, 
      // reset webserver timeout, fire up all sensors and enable LoRaWAN
      webserver_settimeout(wifiSettings.webserverTimeout);
      sensors_init();
#ifdef HAS_LORAWAN_SHIELD
      if (lorawanSettings.enabled)
        lmic_init();
#endif
    }

    // check for NOOP and enter deep sleep
    if (!(runtimeCounterSecs % 10)) {
      checkNOOPTime(settings.beginSleep, settings.endSleep);
      if (co2status == NOOP) {
        webserver_settimeout(WEBSERVER_TIMEOUT_NOOP);
        if (webserver_stop(false)) { // wait for webserver to terminate
          if ((getHourLocal() == settings.endSleep-1) || (getHourLocal() == 23 && !settings.endSleep))
            enterDeepSleep((60-rtc.now().minute())*60);
          else if (getHourLocal() == settings.endSleep)
            // triggered on race condition (system wake up few seconds before endSleep hour)
            // need to enforce a restart; only way to leave NOOP is to re-run setup()
            enterDeepSleep(10);
          else
            enterDeepSleep(3600);
        }
      }
    }

    // skip sensor readings, logging, LoRaWAN transmissions
    // and MQTT push messages when in NOOP mode
    if (co2status != NOOP) {

      if (!(runtimeCounterSecs % mqttSettings.pushInterval)) {
        if (mqttSettings.enabled) {
          if (wifi_uplink(true)) { // reconnect if necessary
            mqtt_send(500);  // will implicitly call mqtt_init()
          } else {
            Serial.println("MQTT: no WiFi uplink, cannot publish readings.");
            logMsg("mqtt failed (no wifi)");
            mqtt_stop();
          }
        } else {
          mqtt_stop();
        }
      }

      // log current sensor readings and check battery
      if (settings.enableLogging && co2status != WARMUP &&
          !(runtimeCounterSecs % settings.loggingInterval)) {
        checkLowBat();
        Serial.print(F("Runtime: "));
        Serial.println(getRuntime(runtimeCounterSecs));
        logReadings(runtimeCounterSecs);
      }

#ifdef HAS_LORAWAN_SHIELD
      if (!(runtimeCounterSecs % 10) && webserver_idle() > 5000) {
        if (lorawanSettings.enabled && !lmic_ready())
          lmic_init();
        else if (!lorawanSettings.enabled && lmic_ready())
          lmic_stop();
      }
      if (lorawanSettings.enabled && !(runtimeCounterSecs % lorawanSettings.txInterval)) {
        if (lmic_ready() && co2status > WARMUP && co2status < CALIBRATE) {
          lmic_send(&lorawanjob);
          waitForLorawanJobs(5, false); // blocking
        }
      }
#endif

      if (scd30_warmup_countdown > 1)
        scd30_warmup_countdown--;

      if (!(runtimeCounterSecs % SCD30_OFFSET_UPDATES_SECS)) {
        scd30_pressure(bme280_pressure);
        scd30_adjustTempOffset();
      }

      // check for log rotation every hour
      if (settings.enableLogging && !(runtimeCounterSecs % 3600))
        rotateLogs();

    } // end !NOOP

    // blinking on ALARM, CALIBRATE, ERROR or NOOP status
    if (co2status >= ALARM) {
      if (leds_on() > 0) {
        if (co2status == NOOP) {
          blink_leds(QUARTER_RING, BLUE, 250, 1, false);

        } else if (co2status == ALARM) {
          blink_leds(HALF_RING, RED, 250, 2, false);
          
        } else if (co2status == FAILURE) {
          blink_leds(HALF_RING, RED, 100, 4, false);
          webserver_start(600); // offer maintenance
          
          if (failureCountdown) // temporarily triggered by calibration failure
            if (!--failureCountdown)
              co2status = NODATA;
              
          if (!failureStateSecs)
            failureStateSecs = millis()/1000;
          if (millis()/1000 - failureStateSecs >= 600)  // 10 min.
            // eventually restart system, might resolve problem
            resetSystem(); 
            
        } else if (co2status == CALIBRATE) {
          blink_leds(HALF_RING, CYAN, 250, 2, false);
          webserver_tickle();
          scd30_calibrate(SCD30_CALIBRATION_SECS);
          if (co2status == FAILURE)
            failureCountdown = 15; // leave failure status after 15 secs
        }
      } else {
        clear_leds(FULL_RING);
      }
    }
    runtimeCounterSecs++;
  }
}
