/***************************************************************************
  Copyright (c) 2020 Lars Wessels

  This file a part of the "CO2-Ampel" source code.
  https://github.com/lrswss/co2ampel

  Published under Apache License 2.0

***************************************************************************/

#include "sensors.h"
#include "led.h"
#include "logging.h"
#include "rtc.h"
#include "utils.h"
#include "config.h"

float scd30_temperature;
uint16_t scd30_co2ppm;
uint8_t scd30_humidity;
int16_t scd30_calibrate_countdown;
int16_t scd30_warmup_countdown;
float bme280_temperature;
uint8_t bme280_humidity;
uint16_t bme280_pressure;
bool hasBME280 = false;
static bool scd30Init = false;
static bool bme280Init = false;
sensorStatus co2status;

char statusNames[9][10] = {
   "nodata",
   "warmup",
   "good",
   "medium",
   "critical",
   "alarm",
   "calibrate",
   "failure",
   "noop"
 };

BME280I2C bme;
SCD30 airsensor;
uEEPROMLib eeprom(0x57);
RunningMedian scd30_co2_readings = RunningMedian(settings.co2MedianSamples);
RunningMedian scd30_co2_calibrate = RunningMedian(15);
RunningMedian scd30_temp_readings = RunningMedian(9);
RunningMedian bme280_temp_readings = RunningMedian(9);


// returns standard deviation of given running median readings
static float stdDev(RunningMedian readings, bool debug) {
  int32_t average = 0;
  uint32_t sqDevSum = 0.0;
  float deviation;

  if (readings.getCount() < 5)
    return UCHAR_MAX;
  
  average = int(readings.getAverage()*100);
  for(uint8_t i = 0; i < readings.getCount(); i++) {
      sqDevSum += pow((average - (readings.getElement(i)*100)), 2);
  }
  deviation = sqrt(sqDevSum/readings.getCount())/100;

  if (debug) {
    Serial.print(F("Values: "));
    for (uint8_t i = 0; i < readings.getCount(); i++) {
      Serial.print(readings.getElement(i));
      Serial.print(" ");
    }
    Serial.println();
    Serial.print(F("Mean: " ));
    Serial.println(average/100.0);
    Serial.print(F("Sigma: "));
    Serial.println(deviation);
  }

  return deviation;
}


// start BME280 and SCD30 sensors
void sensors_init() {
  bme280_init();
  bme280_readings(true);
  scd30_init(bme280_pressure);
  co2status = NODATA;
}


// initaliaze BME-Sensor and return current pressure
uint16_t bme280_init() {
  if (!bme.begin()) {
    Serial.println(F("Couldn't find BME280 sensor...system halted!"));
    logMsg("bme280 error");
    while (1) {
      blink_leds(HALF_RING, RED, 500, 3, false);
      delay(2000);
    }
  }

  switch (bme.chipModel()) {
    case BME280::ChipModel_BME280:
      Serial.println(F("Found BME280 sensor."));
      hasBME280 = true;
      break;
    case BME280::ChipModel_BMP280:
      Serial.println(F("Found BMP280 sensor."));
      break;
    default:
      Serial.println(F("Found UNKNOWN sensor."));
      while (1) {
        blink_leds(HALF_RING, RED, 500, 4, false);
        delay(2000);
      }
  }
  bme280Init = true;
  blink_leds(HALF_RING, GREEN, 100, 2, false);
  delay(1000);
}


void scd30_init(uint16_t pressure) {
  static char buf[64], s[8];
  uint8_t interval;
      
  if (!airsensor.begin(false)) {  // disable auto calibration
    Serial.println(F("Couldn't find scd30 air sensor...system halted!"));
    logMsg("scd30 error");
    while (1) {
      blink_leds(HALF_RING, RED, 500, 5, false);
      delay(2000);
    }
  } else {
    interval = max(3, int(settings.co2ReadingInterval/2));
    Serial.println(F("Found SCD30 air sensor.")); 
    Serial.print(F("SCD30: auto-calibration disabled, reading interval set to "));
    Serial.print(interval);
    Serial.print(F(" secs"));
    sprintf(buf, "scd30 reading interval %d secs", interval);
    if (!airsensor.setMeasurementInterval(interval)) {
      Serial.println(F(" [FAILED]"));
      strcat(buf, " failed");
    }
    Serial.println();
    logMsg(buf);
    
#ifdef ALTITUDE_ABOVE_SEELEVEL
    Serial.print(F("SCD30: set altitude compensation to "));
    Serial.print(settings.altitude);
    Serial.print("m");
    sprintf(buf, "scd30 altitude compensation %dm", settings.altitude);
    if (!airsensor.setAltitudeCompensation(settings.altitude)) {
      Serial.println(F(" [FAILED]"));
      strcat(buf, " failed");
    }
    Serial.println();
    logMsg(buf);
#endif

    if ((pressure > 850) && (pressure < 1050)) {
      Serial.print(F("SCD30: setting ambient pressure to "));
      Serial.print(pressure);
      Serial.print(F("hPa"));
      sprintf(buf, "scd30 ambient pressure %dhPa", pressure);
      if (!airsensor.setAmbientPressure(pressure)) {
        Serial.println(F(" [FAILED]"));
        strcat(buf, " failed");
      }
      Serial.println();
      logMsg(buf);
    }

    airsensor.setTemperatureOffset(settings.scd30TempOffset);
    Serial.print(F("SCD30: temperature offset is "));
    Serial.print(airsensor.getTemperatureOffset(), 2);
    Serial.println();
    dtostrf(airsensor.getTemperatureOffset(), 5, 2, s);
    sprintf(buf, "scd30 temperature offset %s", removeSpaces(s));
    logMsg(buf);
  }
  scd30Init = true;
  blink_leds(HALF_RING, GREEN, 100, 2, false);
  delay(1000);
}


// increase measurement interval to 120 sec. to save power
void scd30_sleep() {
  if (!scd30Init)
    airsensor.begin(false);
  airsensor.setMeasurementInterval(120);
}


// returns true unless we get repeated failures on readings
// set global variables for co2ppm, humidity and temperature 
// and print all readings to console
bool scd30_readings(bool reset) {
  static uint32_t lastReading;
  static uint8_t noCO2Reading;
  uint16_t co2ppm;
  uint8_t retries;
  float stddev;

  if (!scd30Init) {
    Serial.println(F("SCD30: not initialized!"));
    return false;
  }

  if (reset)
    noCO2Reading = 0;
    
  while (retries++ < 4) { // repeat for 2 seconds
    if (airsensor.readMeasurement()) {
      // set global variables
      scd30_temperature = airsensor.getTemperature();
      scd30_humidity = int(airsensor.getHumidity());
      scd30_temp_readings.add(scd30_temperature); // used for temp auto offset

      co2ppm = airsensor.getCO2();
      scd30_co2_readings.add(co2ppm);
      scd30_co2_calibrate.add(co2ppm); // only used for sensor calibration
      if (settings.medianFilter)
        scd30_co2ppm = scd30_co2_readings.getMedian(); // set global variable
      else
        scd30_co2ppm = co2ppm;

      if (co2ppm > 0) {
        lastReading = millis()/1000;
        noCO2Reading = 0;
        Serial.print(F("SCD30: co2("));
        Serial.print(co2ppm);
        Serial.print(F("ppm), co2median("));
        Serial.print(scd30_co2ppm);
#ifdef SCD30_DEBUG
        Serial.print(F("ppm), co2StdDev("));
        Serial.print(stddev = stdDev(scd30_co2_readings, false));     
#endif
        Serial.print(F("ppm), temp("));
        Serial.print(scd30_temperature, 1);
#ifdef SCD30_DEBUG
        Serial.print(F("C), tempStdDev("));
        Serial.print(stdDev(scd30_temp_readings, false));
#endif       
        Serial.print(F("C), hum("));
        Serial.print(scd30_humidity, 1);
        Serial.println(F("%)"));
      } else {
        blink_leds(SYSTEM_LED2, RED, 100, 2, true);
        Serial.println(F("SCD30: invalid CO2 reading"));
        noCO2Reading++;
        return true; // will trigger state NODATA
      }
      return true;
    }
    Serial.println(F("SCD30: no data"));
    noCO2Reading++;
    delay(500);
  }

  // try a soft reset after 10 consecutive failures to read CO2 value
  // if this doesn't help reading timeout will eventually be triggered
  // and system will switch into state FAILURE
  if (noCO2Reading == 10) { 
    Serial.println(F("SCD30: soft reset"));
    logMsg("scd30 invalid or no co2 readings, soft reset");
    if (!scd30_softreset())
      return false; // switch to FAILURE status
  }
  
  if ((millis()/1000 - lastReading) >= SCD30_READING_TIMEOUT) {
    Serial.println(F("SCD30: reading failed repeatedly!"));
    if (co2status != FAILURE)
      logMsg("scd30 reading failed");
    return false;  // trigger FAILURE
  }
  return true;
}


// update ambient pressure value on significant changes
void scd30_pressure(uint16_t pressure) {
  static uint16_t lastPressure = 0;
  static char buf[64];

  if (!scd30Init) {
    Serial.println(F("SCD30: not initialized!"));
    return;
  }

  if (lastPressure > 0 && (pressure > 850) && (pressure < 1050) 
      && (abs(lastPressure - pressure) > 5)) {
    sprintf(buf, "scd30 ambient pressure %d -> %d", lastPressure, pressure);
    Serial.print(F("SCD30: update ambient pressure to "));
    Serial.print(pressure);
    Serial.print(F(" hPa"));
    if (!airsensor.setAmbientPressure(pressure)) {
      Serial.print(F(" [FAILED]"));
      strcat(buf, " failed");
    }
    Serial.println();
    logMsg(buf);
  }
  lastPressure = pressure;
}


void scd30_adjustTempOffset() {
  static char buf[64], s1[8], s2[8];
  float tempDiff, tempOffset, stddev;

  if (!scd30Init) {
    Serial.println(F("SCD30: not initialized!"));
    return;
  } 
  
  if (scd30_temp_readings.getCount() < scd30_temp_readings.getSize()*0.9)
    return;
  
  if (stdDev(scd30_temp_readings, false) > 0.08 || stdDev(bme280_temp_readings, false) > 0.89)
    return; 

  tempDiff = airsensor.getTemperature() - bme280_temperature;
  tempOffset = airsensor.getTemperatureOffset() + (tempDiff*0.95);
  if (tempOffset < 0)
    tempOffset = 0; // offset must be positive
    
  if (fabs(tempDiff) >= 0.2) {
    Serial.print(F("SCD30: adjusting temperature offset from "));
    Serial.print(airsensor.getTemperatureOffset());
    Serial.print(" to ");
    Serial.print(tempOffset);
    dtostrf(tempOffset, 5, 2, s1);
    dtostrf(airsensor.getTemperatureOffset(), 5, 2, s2);
    sprintf(buf, "scd30 temperature offset %s -> %s", removeSpaces(s2), removeSpaces(s1));
    if (!airsensor.setTemperatureOffset(tempOffset)) {
      Serial.print(F(" [FAILED]"));
      strcat(buf, " failed");
    }
    Serial.println();
    logMsg(buf);
  
  } else {
    dtostrf(airsensor.getTemperatureOffset(), 5, 2, s1);
    Serial.printf("SCD30: no temperature offset change, keeping %s\n", removeSpaces(s1));
  }
}


// triggers calibration of the SCD30; sensor must be exposed 
// to fresh air (outside); if sensor readings are stable it's 
// set to SCD30_CO2_CALIBRATION_VALUE as new baseline value
void scd30_calibrate(uint16_t timeoutSecs) {
  static char buf[64], s[8];
  static uint16_t prevCheckSecs = millis()/1000;
  static double stddev;

  if (!scd30Init) {
    Serial.println(F("SCD30: not initialized!"));
    return;
  }

  if (co2status != CALIBRATE) {
    co2status = CALIBRATE;
    scd30_calibrate_countdown = timeoutSecs;
    scd30_co2_calibrate.clear();
    scd30_readings(true);
    stddev = UCHAR_MAX;
    airsensor.setMeasurementInterval(2); // max. SCD30 reading interval
    Serial.print(F("SCD30: start calibration for "));
    Serial.print(timeoutSecs);
    Serial.print(F(" secs with target value "));
    Serial.print(SCD30_CO2_CALIBRATION_VALUE);
    Serial.println(F("ppm..."));
    sprintf(buf, "scd30 start calibration (%dsec, %dppm)", timeoutSecs, SCD30_CO2_CALIBRATION_VALUE);
    logMsg(buf);
    
  } else {

    scd30_calibrate_countdown -= (millis()/1000 - prevCheckSecs);
    prevCheckSecs = millis()/1000;

    // add new reading to running median every 10 secs
    // and calculate standard deviation
    if (scd30_calibrate_countdown % 10 <= 1) {
      scd30_readings(true);
      // start calculating standard deviation after 2 min.
      if (scd30_co2_calibrate.getCount() >= 12)
        stddev = stdDev(scd30_co2_calibrate, true);
      dtostrf(stddev, 5, 2, s);
    }

    // didn't stable reading within given timeout => exit with status FAILURE
    if ((scd30_calibrate_countdown <= 0) && (stddev > 3)) {
      co2status = FAILURE;
      scd30_calibrate_countdown = 0;
      airsensor.setMeasurementInterval(int(settings.co2ReadingInterval/2));
      Serial.printf("SCD30: calibration timeout, standard deviation %s too high\n", removeSpaces(s));
      sprintf(buf, "scd30 calibration timeout, sigma %s", removeSpaces(s));
      logMsg(buf);

    // running median is below standard deviation threshold => recalibrate sensor
    } else if (stddev <= 3) {
      scd30_calibrate_countdown = 0;
      airsensor.setMeasurementInterval(int(settings.co2ReadingInterval/2));

      if (airsensor.setForcedRecalibrationFactor(SCD30_CO2_CALIBRATION_VALUE)) {
        co2status = NODATA;
        Serial.printf("SCD30: calibration successful, %dppm -> %dppm, sigma %s\n", 
          int(scd30_co2_calibrate.getAverage()), SCD30_CO2_CALIBRATION_VALUE, removeSpaces(s));
        sprintf(buf, "scd30 calibration ok, %dppm -> %dppm, sigma %s", 
          int(scd30_co2_calibrate.getAverage()), SCD30_CO2_CALIBRATION_VALUE, removeSpaces(s));
        logMsg(buf);
        
      } else {
        co2status = FAILURE;
        Serial.println(F("SCD30: calibration failed!"));
        logMsg("scd30 calibration failed");
      }
    }
  }
}


// Sensirion_CO2_Sensors_SCD30_Interface_Description.pdf (May 2020)
// not implemented in SparkFun library
bool scd30_softreset() {
  if (!scd30Init) {
    Serial.println(F("SCD30: not initialized!"));
    return false;
  }
  return airsensor.sendCommand(0xD304);
}


// set global pressure variable and print all
// BME/BMP280 readings (temp, hum, pres) to console
void bme280_readings(bool verbose) {
  uint16_t pres;

  if (!bme280Init)
    bme280_init();

  bme280_pressure = int(bme.pres()/100); // global variable
  bme280_temperature = bme.temp(BME280::TempUnit_Celsius);
  if (hasBME280)
    bme280_humidity = int(bme.hum());

  if (!verbose)
    return;

  if (hasBME280) {
    Serial.print(F("BME280: hum("));
    Serial.print(bme280_humidity);
    Serial.print("%), ");
  } else {
    Serial.print(F("BMP280: "));
  }
  Serial.print(F("temp("));
  Serial.print(bme280_temperature, 1);
  bme280_temp_readings.add(bme280_temperature); // used for temp auto offset
#ifdef SCD30_DEBUG
  Serial.print(F("C), tempStdDev("));
  Serial.print(stdDev(bme280_temp_readings, false));
#endif  
  Serial.print(F("C), pres("));
  Serial.print(bme280_pressure);
  Serial.println(F("hPa)"));
}
