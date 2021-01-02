/***************************************************************************
  Copyright (c) 2020-2021 Lars Wessels

  This file a part of the "CO2-Ampel" source code.
  https://github.com/lrswss/co2ampel

  Published under Apache License 2.0

***************************************************************************/

#ifndef _WEBSERVER_H
#define _WEBSERVER_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "config.h"

#define WEBSERVER_TIMEOUT_MIN_SECS 90
#define WEBSERVER_TIMEOUT_MAX_SECS 1800
#define WEBSERVER_TIMEOUT_NOOP 60

extern ESP8266WebServer webserver;

void webserver_start(uint16_t timeout);
bool webserver_stop(bool force);
void webserver_settimeout(uint16_t timeoutSecs);
void webserver_tickle();
uint32_t webserver_idle();

#endif  
