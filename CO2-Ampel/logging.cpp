/***************************************************************************
  Copyright (c) 2020-2021 Lars Wessels

  This file a part of the "CO2-Ampel" source code.
  https://github.com/lrswss/co2ampel

  Published under Apache License 2.0

***************************************************************************/

#include "logging.h"
#include "webserver.h"
#include "sensors.h"
#include "utils.h"
#include "rtc.h"
#include "config.h"

#ifdef LANG_EN
#include "html_EN.h"
#else
#include "html_DE.h"
#endif

static bool fsInited = true;

void mountFS() {
  FSInfo fs_info;
  uint32_t freeBytes;
  
  if (LittleFS.begin()) {
    LittleFS.info(fs_info);
    freeBytes = fs_info.totalBytes * 0.95 - fs_info.usedBytes;
    Serial.print(F("LittleFS mounted: "));
    Serial.print(freeBytes/1024);
    Serial.println(F(" kb free"));
    listDirectory("/");
    rotateLogs();
    fsInited = true;
  } else {
    Serial.println(F("Failed to mount LittleFS!"));
  }
}


void logMsg(char *msg) {
  File logfile;
  uint32_t epoch = 0;
  char timeStr[20];
  DateTime now;
  
  if (!settings.enableLogging || !fsInited)
    return;

  if (rtcOK) {
    now = rtc.now();
    epoch = now.unixtime();
  }
  time_t t = CE.toLocal(epoch);
  logfile = LittleFS.open(LOGFILE_NAME, "a");
  if (logfile) {
    sprintf(timeStr, "%4d-%.2d-%.2dT%.2d:%.2d:%.2d", 
        year(t), month(t), day(t), hour(t), minute(t), second(t));
    logfile.print(timeStr);
    logfile.print(",");
    logfile.println(msg);
    logfile.close();
  }
}


void logReadings(uint32_t runtimeSecs) {
  static char csv[64], buf[16];

  if (co2status == NOOP || !settings.enableLogging)
    return;

  sprintf(csv, "%d,%s,", runtimeSecs, statusNames[co2status]);
  itoa(scd30_co2ppm, buf, 10);
  strcat(csv, buf);
  strcat(csv, ",");
  dtostrf(scd30_temperature, 6, 2, buf);
  strcat(csv, buf);
  strcat(csv, ",");
  itoa(scd30_humidity, buf, 10);
  strcat(csv, buf);
  strcat(csv, ",");
  dtostrf(bme280_temperature, 6, 2, buf);
  strcat(csv, buf);
  strcat(csv, ",");
  itoa(bme280_humidity, buf, 10);
  strcat(csv, buf);
  strcat(csv, ",");
  itoa(bme280_pressure, buf, 10);
  strcat(csv, buf);
  strcat(csv, ","); 
  dtostrf(getVBAT(), 5, 2, buf);
  strcat(csv, buf);
  
  logMsg(removeSpaces(csv));
  Serial.println(F("Readings logged."));
}


bool handleSendFile(String path) {
  if (!settings.enableLogging || !fsInited)
    return false;

  if (LittleFS.exists(path) && strstr(path.c_str(), LOGFILE_NAME) != NULL) {
    File file = LittleFS.open(path, "r");
    if (file) {
      logMsg("send log");
      Serial.printf("Sending file %s (%d bytes)...\n", file.name(), file.size());
      webserver.streamFile(file, "text/plain");
      file.close();
      return true;
    }
  }
  return false;
}


void listDirectory(const char* dir) {
  File file;
  Dir rootDir;
  
  rootDir = LittleFS.openDir(dir);
  Serial.println(F("Contents of root directory: "));
  while (rootDir.next()) {
    file = rootDir.openFile("r");
    Serial.print(file.name());
    Serial.print(" (");
    Serial.print(file.size());
    Serial.println(F(" bytes)"));
    file.close();
  }
}


String listDirHTML(const char* path) {
  String listing, filename;
  Dir rootDir;
  File file;

  if (!settings.enableLogging || !fsInited)
    return listing;

  rootDir = LittleFS.openDir("/");
  while (rootDir.next()) {
    filename = rootDir.fileName();
    file = rootDir.openFile("r");
    listing += "<a href=\"";
    listing += filename;
    listing += "\">";
    listing += filename;
    listing += "</a> (";
    listing += file.size();
    listing += " bytes)<br>\n";
    file.close();
  }
  return listing;
}


void removeLogs() {
  String filename;
  Dir rootDir;

  if (!settings.enableLogging || !fsInited)
    return;

  rootDir = LittleFS.openDir("/");
  while (rootDir.next()) {
    filename = rootDir.fileName();
    Serial.print(F("Removing file "));
    Serial.print(filename);
    Serial.println(F("..."));
    LittleFS.remove(filename);
    delay(100);
  }
}

void rotateLogs() {
  String fOld, fNew;
  int maxFiles = 0;
  FSInfo fs_info;

  if (!settings.enableLogging || !fsInited)
    return;

  LittleFS.info(fs_info);
  File file = LittleFS.open(LOGFILE_NAME, "r");
  if (file && file.size() > LOGFILE_MAX_SIZE) {
    file.close();
    maxFiles = int(fs_info.totalBytes * 0.95 / LOGFILE_MAX_SIZE) - 1;
    maxFiles = max(LOGFILE_MAX_FILES, maxFiles);
    LittleFS.remove(String(LOGFILE_NAME) + "." + LOGFILE_MAX_FILES); // remove oldest log file
    for (uint8_t i = maxFiles; i > 1; i--) {
      fOld = String(LOGFILE_NAME) + "." + String(i - 1);
      fNew = String(LOGFILE_NAME) + "." + i;
      if (LittleFS.exists(fOld)) {
        Serial.print(fOld); Serial.print(" -> "); Serial.println(fNew);
        LittleFS.rename(fOld, fNew);
        delay(10);
      }
    }
    fOld = String(LOGFILE_NAME);
    fNew = String(LOGFILE_NAME) + "." + 1;
    Serial.print(fOld); Serial.print(" -> "); Serial.println(fNew);
    LittleFS.rename(fOld, fNew);
  }
}


// concate all log files into one http content stream
void sendAllLogs() {
  File file;
  String logFile, downloadFile;
  WiFiClient client = webserver.client();
  uint32_t totalSize = 0;

  if (!settings.enableLogging || !fsInited)
    return;

  // determine total size of all files (for content-size header)
  file = LittleFS.open(LOGFILE_NAME, "r");
  if (file)
    totalSize = file.size();
  for (uint8_t i = LOGFILE_MAX_FILES; i >= 1; i--) {
    logFile = String(LOGFILE_NAME) + "." + i;
    file = LittleFS.open(logFile, "r");
    if (file)
      totalSize += file.size();
  }
  Serial.printf("Sending all logs in one file (%d bytes)...\n", totalSize);

  // send header for upcoming byte stream
  downloadFile = "co2ampel_" + systemID() + ".log";
  webserver.sendHeader("Content-Type", "text/plain");
  webserver.sendHeader("Content-Disposition", "attachment; filename="+downloadFile);
  webserver.setContentLength(totalSize);
  webserver.sendHeader("Connection", "close");
  webserver.send(200, "application/octet-stream","");

  // finally stream all files to client in one chunck
  for (uint8_t i = LOGFILE_MAX_FILES; i >= 1; i--) {
    logFile = String(LOGFILE_NAME) + "." + i;
    file = LittleFS.open(logFile, "r");
    if (file)
      client.write(file);
  }
  file = LittleFS.open(LOGFILE_NAME, "r");
  if (file)
    client.write(file);
  client.flush();
}
