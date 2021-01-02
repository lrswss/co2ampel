/***************************************************************************
  Copyright (c) 2020-2021 Lars Wessels

  This file a part of the "CO2-Ampel" source code.
  https://github.com/lrswss/co2ampel

  Published under Apache License 2.0

***************************************************************************/

#ifndef _LOGGING_H
#define _LOGGING_H

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

#define LOGFILE_MAX_SIZE 1024*50  // 50k
#define LOGFILE_MAX_FILES 24
#define LOGFILE_NAME "/sensor.log"

void mountFS();
void logMsg(char *msg);
void listDirectory(const char* dir);
void sendAllLogs();
void rotateLogs();
void removeLogs();
void logReadings(uint32_t runtimeSecs);
bool handleSendFile(String path);
String listDirHTML(const char* path);

#endif
