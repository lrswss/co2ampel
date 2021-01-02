/***************************************************************************
  Copyright (c) 2020-2021 Lars Wessels

  This file a part of the "CO2-Ampel" source code.
  https://github.com/lrswss/co2ampel

  Published under Apache License 2.0

***************************************************************************/

#ifndef _SENSORS_H
#define _SENSORS_H

#include "SparkFun_SCD30_Arduino_Library.h"
#include <BME280I2C.h>
#include <Wire.h>
#include <RunningMedian.h>

#define SCD30_CO2_CALIBRATION_VALUE 420
#define SCD30_CALIBRATION_SECS 300
#define SCD30_TEMP_OFFSET 1.9
#define SCD30_OFFSET_UPDATES_SECS 600
#define SCD30_INTERVAL_MIN_SECS 5
#define SCD30_INTERVAL_MAX_SECS 60 // see CD_AN_SCD30_Low_Power_Mode_D2.pdf
#define SCD30_WARMUP_SECS 60
#define SCD30_READING_TIMEOUT 90
#define CO2_LOWER_BOUND 350  // https://wiki.seeedstudio.com/Grove-CO2_Sensor/


enum sensorStatus {
  NODATA,
  WARMUP,
  GOOD,
  MEDIUM,
  CRITICAL,
  ALARM,
  CALIBRATE,
  FAILURE,
  NOOP
};

extern uint16_t scd30_co2ppm;
extern float scd30_temperature;
extern uint8_t scd30_humidity;
extern int16_t scd30_calibrate_countdown;
extern int16_t scd30_warmup_countdown;
extern float bme280_temperature;
extern uint16_t bme280_pressure;
extern uint8_t bme280_humidity;
extern bool hasBME280;
extern sensorStatus co2status;
extern char statusNames[9][10];

extern BME280I2C bme;
extern SCD30 airSensor;
extern RunningMedian scd30_co2_readings;

void sensors_init();
uint16_t bme280_init();
void bme280_readings(bool verbose);
void scd30_init(uint16_t pressure);
void scd30_sleep();
bool scd30_readings(bool reset);
void scd30_pressure(uint16_t pressure);
void scd30_adjustTempOffset();
void scd30_calibrate(uint16_t timeout);
bool scd30_softreset();
#endif
