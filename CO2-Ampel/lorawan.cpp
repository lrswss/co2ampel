/***************************************************************************
  Copyright (c) 2020-2021 Lars Wessels

  This file a part of the "CO2-Ampel" source code.
  https://github.com/lrswss/co2ampel

  Published under Apache License 2.0

***************************************************************************/

#include "lorawan.h"
#include "led.h"
#include "sensors.h"
#include "rtc.h"
#include "logging.h"
#include "utils.h"


#ifdef HAS_LORAWAN_SHIELD
static const uint32_t NETID = LORAWAN_NETID; // fixed (see config.h)
static const uint8_t PROGMEM DEVEUI[8] = LORAWAN_DEVEUI;  // fixed, lsb (see config.h)

#if defined(LORAWAN_DEVADDR) && defined(LORAWAN_NWKSKEY) && defined(LORAWAN_APPSKEY)
static const uint32_t DEVADDR = LORAWAN_DEVADDR;  // msb
static const uint8_t PROGMEM NWKSKEY[16] = LORAWAN_NWKSKEY;  // msb
static const uint8_t PROGMEM APPSKEY[16] = LORAWAN_APPSKEY;  // msb
#endif

#if defined(LORAWAN_APPEUI) && defined(LORAWAN_APPKEY)
static const uint8_t PROGMEM APPEUI[8] = LORAWAN_APPEUI; // lsb
static const uint8_t PROGMEM APPKEY[16] = LORAWAN_APPKEY;  // msb
#endif

#ifdef LORAWAN_OBSCURE_KEYS
static bool lorawanObscureKeys = true;
#else
static bool lorawanObscureKeys = false;
#endif

osjob_t lorawanjob;
session_t lorawanSession;
loraprefs_t lorawanSettings;
bool lmicInited = false;

// since pin16 is used for deep sleep wakup
// we need to borrow pin2 from unsed button
const lmic_pinmap lmic_pins = {
  .nss = 2,
  .rxtx = LMIC_UNUSED_PIN,
  .rst = LMIC_UNUSED_PIN,
  .dio = { 15, 15, LMIC_UNUSED_PIN },
};


void os_getDevEui (uint8_t* buf) {
  memcpy_P(buf, DEVEUI, 8);  // preset in config.h
}

void os_getArtEui (uint8_t* buf) {
  memcpy_P(buf, lorawanSettings.appEui, 8);
}

void os_getDevKey (uint8_t* buf) {
  memcpy_P(buf, lorawanSettings.appKey, 16);
}


// load presets from config.h
static void setDefaults(loraprefs_t *s) {
  // an explicit memset is required to ensure zeroed padding
  // bytes in struct which would otherwise break CRC
  memset(s, 0, sizeof(*s));

  s->txInterval = LORAWAN_TX_INTERVAL_SECS;
  s->drJoin = LORAWAN_DRJOIN;
  s->drSend = LORAWAN_DRTX;
  s->netid = LORAWAN_NETID;
#ifdef ENABLE_LORAWAN
  s->enabled = true;
#endif
#ifdef LORAWAN_USETTN
  s->useTTN = true;
#endif
#ifdef LORAWAN_USEOTAA
  s->useOTAA = true;
#endif
#if defined(LORAWAN_DEVADDR) && defined(LORAWAN_NWKSKEY) && defined(LORAWAN_APPSKEY)
  lorawanSettings.devAddr = LORAWAN_DEVADDR;
  memcpy_P(s->appsKey, APPSKEY, 16);
  memcpy_P(s->nwksKey, NWKSKEY, 16);
#endif
#if defined(LORAWAN_APPEUI) && defined(LORAWAN_APPKEY)
  memcpy_P(s->appEui, APPEUI, 8);
  memcpy_P(s->appKey, APPKEY, 16);
#endif
  s->crc = crc16((uint8_t *) s, offsetof(loraprefs_t, crc));
}


// print settings for debugging
static bool printLoRaWANSettings(loraprefs_t *s) {
#ifdef SETTINGS_DEBUG
  Serial.printf("Enabled: %d\n", s->enabled);
  Serial.printf("TxInterval: %d\n", s->txInterval);
  Serial.printf("DrJoin: %d\n", s->drJoin);
  Serial.printf("DrSend: %d\n", s->drSend);
  Serial.printf("UseTTN: %d\n", s->useTTN);
  Serial.printf("UseOTAA: %d\n", s->useOTAA);
  Serial.print(F("Netid: "));
  Serial.println(s->netid, HEX);
  Serial.print(F("DevAddr: "));
  Serial.println(s->devAddr, HEX);
  Serial.print(F("NwksKey: "));
  printHEX8bit(s->nwksKey, 16, true, false, lorawanObscureKeys);
  Serial.print(F("AppsKey: "));
  printHEX8bit(s->appsKey, 16, true, false, lorawanObscureKeys);
  Serial.print(F("AppEui: "));
  printHEX8bit(s->appEui, 8, true, true, lorawanObscureKeys);
  Serial.print(F("AppKey: "));
  printHEX8bit(s->appKey, 16, true, false, lorawanObscureKeys);
  Serial.printf("CRC: %d\n", s->crc);
#endif
  return true;
}


static void printDataRate() {
  switch (LMIC.datarate) {
    case DR_SF12: Serial.print(F("SF12")); break;
    case DR_SF11: Serial.print(F("SF11")); break;
    case DR_SF10: Serial.print(F("SF10")); break;
    case DR_SF9: Serial.print(F("SF9")); break;
    case DR_SF8: Serial.print(F("SF8")); break;
    case DR_SF7: Serial.print(F("SF7")); break;
    case DR_SF7B: Serial.print(F("SF7B")); break;
    case DR_FSK: Serial.print(F("FSK")); break;
    default: Serial.print(F("unknown"));  break;
  }
}


static void printLMICVersion() {
  Serial.print(ARDUINO_LMIC_VERSION_GET_MAJOR(ARDUINO_LMIC_VERSION));
  Serial.print(F("."));
  Serial.print(ARDUINO_LMIC_VERSION_GET_MINOR(ARDUINO_LMIC_VERSION));
  Serial.print(F("."));
  Serial.print(ARDUINO_LMIC_VERSION_GET_PATCH(ARDUINO_LMIC_VERSION));
  Serial.print(F("."));
  Serial.println(ARDUINO_LMIC_VERSION_GET_LOCAL(ARDUINO_LMIC_VERSION));
}


static void printSessionKeys() {
#ifdef LORAWAN_DEBUG
  Serial.print(F("Netid: 0x"));
  Serial.println(LMIC.netid, HEX);
  Serial.print(F("Device Address: "));
  Serial.println(LMIC.devaddr, HEX);
  Serial.print(F("Network Session Key: "));
  printHEX8bit(LMIC.nwkKey, 16, true, false, lorawanObscureKeys);
  Serial.print(F("App Session Key: "));
  printHEX8bit(LMIC.artKey, 16, true, false, lorawanObscureKeys);
  Serial.flush();
#endif
}


// print (preset) OTAA authentication keys
static void printOTAAKeys() {
#ifdef LORAWAN_DEBUG
  uint8_t buf[16];

  os_getDevEui(buf);
  Serial.print(F("Device EUI: ")); // stored in flash, cannot be changed
  printHEX8bit(buf, 8, true, true, false);
  os_getArtEui(buf);
  Serial.print(F("Application EUI: "));
  printHEX8bit(buf, 8, true, true, false);
  os_getDevKey(buf);
  Serial.print(F("Application Key: "));
  printHEX8bit(buf, 16, true, false, lorawanObscureKeys);
  Serial.flush();
#endif
}


// initialize LMIC library
void lmic_init() {
  char buf[32];
  
  if (lmicInited)
    return;

  save_leds();
  Serial.print(F("Init MCCI LoRaWAN LMIC Library Version "));
  printLMICVersion();

  if (!lorawanSettings.useOTAA && !lorawanSettings.devAddr) {
    Serial.println(F("Incompleted ABP settings, LoRaWAN setup failed!"));
    logMsg("lmic failed, abp keys missing");
    return;
  } else if (lorawanSettings.useOTAA && !array2int(lorawanSettings.appEui, 8)) {
    Serial.println(F("Incompleted OTAA settings, LoRaWAN setup failed!"));
    logMsg("lmic failed, otaa keys missing");
    return;
  }

  // need to zero padding bytes in struct or CRC16 will 
  // break when saving/restoring LMIC to/from EEPROM
  memset(&LMIC, 0, sizeof(lmic_t)); 
  
  os_init();
  LMIC_reset(); // reset the MAC state
  LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100);
  loadLoRaWANSession(); // try to restore previous ABP/OTAA session from EEPROM
  printSessionKeys();

  if (!lorawanSettings.useOTAA && !LMIC.seqnoUp) {
    // set up the channels used by the Things Network, which corresponds
    // to the defaults of most gateways. Without this, only 3 base hannels
    // from the LoRaWAN specification are used
    Serial.println(F("Using 868MHz ISM-Band with 8 channel setup for TTN."));
    LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);
    LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI);
    LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);
    LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);
    LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);
    LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);
    LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);
    LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);

    LMIC_setSession(lorawanSettings.netid, lorawanSettings.devAddr,
      lorawanSettings.nwksKey, lorawanSettings.appsKey);

    // LoRa Alliance NetID Allocation Eligibility Policy and Application Form v3.0:
    // "In any case, devices with fixed hardcoded data rates of SF12 or SF11 must
    // not be allowed to join the network."
    if (lorawanSettings.useTTN && lorawanSettings.drSend < DR_SF10) {
      Serial.println(F("Limit fixed datarate to SF10 on TTN."));
      lorawanSettings.drSend = DR_SF10;
    }

    // TTN uses SF9 for its RX2 window.
    if (lorawanSettings.useTTN) {
      Serial.println(F("Setting SF9 for RX2 window."));
      LMIC.dn2Dr = DR_SF9;
    }

    // prevent EV_LINK_DEAD if there are no frequent DN messages
    // LMIC_setLinkCheckMode() must be called only if a session is established
    Serial.println(F("Disable LinkCheckMode."));
    LMIC_setLinkCheckMode(0);
  }

  if (!LMIC.seqnoUp && !lorawanSettings.useOTAA) {
    LMIC_setDrTxpow(lorawanSettings.drSend, LORAWAN_TXPOWER);
    Serial.print(F("Set DR for TX to "));
    printDataRate();
    Serial.println(".");
  }

  Serial.printf("Sensor readings will be transmitted over LoRaWAN every %d secs.\n", lorawanSettings.txInterval);
  sprintf(buf, "lorawan started, txinterval %d secs", lorawanSettings.txInterval);
  logMsg(buf);
  blink_leds(SYSTEM_LEDS, MAGENTA, 100, 2, true);
  lmicInited = true;
  delay(500);

  if (!LMIC.seqnoUp && lorawanSettings.useOTAA) {
    LMIC_sendAlive(); // also triggers OTAA join
    waitForLorawanJobs(10, true);
  }
}


void onEvent (ev_t event) {
  char buf[32];

  save_leds();
  switch (event) {
    case EV_JOINING:
      Serial.println(F("Starting OTAA join..."));
      logMsg("lorawan joining");
      LMIC_setDrTxpow(lorawanSettings.drJoin, LORAWAN_TXPOWER); // use higher DR for joining
      Serial.print(F("Set DR for OTAA join to "));
      printDataRate();
      Serial.println(".");
      printOTAAKeys();
      break;
    case EV_JOIN_TXCOMPLETE:
      Serial.println(F("OTAA join request sent."));
      break;
    case EV_JOINED:
      Serial.println(F("OTAA join successful."));
      logMsg("lorawan joined");
      blink_leds(SYSTEM_LED1, GREEN, 100, 2, true);
      Serial.println(F("Disable LinkCheckMode."));
      LMIC_setLinkCheckMode(0); // avoid EV_LINK_DEAD without DN
      LMIC_setDrTxpow(lorawanSettings.drSend, LORAWAN_TXPOWER); // reset to default DR
      Serial.print(F("Set DR for TX to "));
      printDataRate();
      Serial.println(".");
      printSessionKeys();
      saveLoRaWANSession();
      saveLoRaWANSettings(false);
      break;
    case EV_JOIN_FAILED:
      Serial.println(F("OTAA join failed!"));
      logMsg("lorawan join failed");
      blink_leds(SYSTEM_LED1, RED, 50, 4, true);
      break;
    case EV_REJOIN_FAILED:
      Serial.println(F("Rejoin failed!"));
      logMsg("lorawan rejoin failed");
      break;
    case EV_TXCOMPLETE:
      Serial.println(F("TX completed (including RX windows)."));
      sprintf(buf, "lorawan tx, seqno %d", LMIC.seqnoUp);
      logMsg(buf);
      blink_leds(SYSTEM_LED1, GREEN, 100, 2, true);
      if (LMIC.txrxFlags & TXRX_ACK)
        Serial.println(F("Received LoRaWAN ACK messages."));
      saveLoRaWANSession();
      break;
    case EV_RESET:
      Serial.println(F("EV_RESET"));
      break;
    case EV_LINK_DEAD:
      Serial.println(F("EV_LINK_DEAD"));
      break;
    case EV_LINK_ALIVE:
      Serial.println(F("EV_LINK_ALIVE"));
      break;
    case EV_TXSTART:
      Serial.print(F("TX started ("));
      Serial.print(LMIC.freq);
      Serial.print(F(", "));
      printDataRate();
      Serial.println(F(")..."));
      break;
    case EV_TXCANCELED:
      Serial.println(F("EV_TXCANCELED"));
      break;
    default:
      Serial.print(F("Unknown LMIC event: "));
      Serial.println((unsigned) event);
      break;
  }
}


// send off payload with sensor data
void lmic_send(osjob_t* job) {
  uint8_t i = 1;
  uint8_t payload[15];
  uint16_t temp;

  if (!lorawanSettings.enabled)
    return;

  // Check if there is not a current TX/RX job running
  if (LMIC.opmode & OP_TXRXPEND) {
    Serial.println(F("OP_TXRXPEND, not sending"));
    return;
  } else {
    // prepare payload for uplink transmission
    payload[i++] = 0x01;
    payload[i++] = byte(co2status & 0xff);

    payload[i++] = 0x10;
    temp = int(bme280_temperature * 10);
    payload[i++] = byte(temp >> 8);
    payload[i++] = byte(temp & 0xff);

    // 0-100%
    payload[i++] = 0x11;
    payload[i++] = byte(int(bme280_humidity) & 0xff);

    // range from low 870hPa to high 1085hPa fit's into single byte value
    payload[i++] = 0x12;
    payload[i++] = byte((bme280_pressure / 100 - 870) & 0xff);

    // SCD30 value range from 0 to 40000ppm
    payload[i++] = 0x13;
    payload[i++] = byte(scd30_co2ppm >> 8);
    payload[i++] = byte(scd30_co2ppm & 0xff);

    // battery voltage
    payload[i++] = 0x20;
    payload[i++] = int(getVBAT()*100) - 256;

    // payload size serves as simple check sum
    payload[0] = i;

    Serial.printf("LoRaWAN packet %d (", LMIC.seqnoUp);
    printHEX8bit(payload, sizeof(payload), false, false, false);
    Serial.println(F(") queued."));
    LMIC_setTxData2(1, payload, i, 0);  // no ack
    save_leds();
    blink_leds(SYSTEM_LED1, MAGENTA, 250, 1, true);
  }
}


void lmic_stop() {
  if (!lmicInited)
    return;
  lmicInited = false;
  saveLoRaWANSession();
  LMIC_shutdown();
  Serial.println(F("LoRaWAN stopped."));
  logMsg("lorawan stopped");
}


bool lmic_ready() {
  return lmicInited;
}


// wait for pending LoRaWAN jobs for given number of seconds
// optionaly toggle LED while waiting
void waitForLorawanJobs(uint8_t secs, bool toggleLED) {
  uint32_t wait = 0;

  while (lmicInited && ((LMIC.opmode & (OP_TXRXPEND|OP_JOINING|OP_POLL)) ||
          os_queryTimeCriticalJobs(ms2osticks(secs * 1000)))) {
    if (wait == 2500)
      Serial.println(F("LoRaWAN jobs pending, waiting..."));
    if (toggleLED && (wait > 1000) && !(wait % 250))
      toggle_leds(SYSTEM_LED1, MAGENTA);
    if (wait > secs * 1000)
      break;
    os_runloop_once();
    wait++;
    delay(1);
  }
  clear_leds(SYSTEM_LED1);
  delay(500);
}


// remap LiIon battery voltage (3.45 - 4.2V) to 1-254
uint8_t os_getBattLevel(void) {
  return (uint8_t) map(getVBAT() * 1000, 3450, 4200, MCMD_DEVS_BATT_MIN, MCMD_DEVS_BATT_MAX);
}


// load session data from EEPROM or if not available start new session
void loadLoRaWANSession() {
  session_t buf;
  bool error = false;

  Serial.printf("Loading LoRaWAN session data (%d bytes) from EEPROM...", sizeof(lorawanSession));
  memset(&buf, 0, sizeof(buf));
  if (!rtceeprom.eeprom_read(EEPROM_LORAWAN_SESSION_ADDR, (byte *) &buf, sizeof(buf))) {
    logMsg("load lorawan session failed");
    Serial.println(F("failed!"));
    error = true;
  }

  if (!error && crc16((uint8_t *) &buf.lmic, sizeof(buf.lmic)) != buf.crc) {
    if (buf.crc > 0) {
      logMsg("load lorawan session crc error");
      Serial.print(F("crc error, "));
    }
    error = true;
  } else {
    memcpy((void*)&lorawanSession, &buf, sizeof(lorawanSession));
  }

  if (!error && lorawanSession.lmic.seqnoUp != 0) {
    Serial.printf("continue %s with seqnoUp %d.\n",
      (lorawanSettings.useOTAA ? "OTAA" : "ABP"), lorawanSession.lmic.seqnoUp);
#if defined(CFG_LMIC_EU_like) 
    // need to reset the duty cycle limits (off due to intermediate deep sleep cycles)
    for (uint8_t i = 0; i < MAX_BANDS; i++)
      lorawanSession.lmic.bands[i].avail = 0;
    lorawanSession.lmic.globalDutyAvail = 0;
#endif
    LMIC = lorawanSession.lmic;
    LMIC_clrTxData(); // delete pending jobs
  } else {
    Serial.printf("starting new %s session.\n", (lorawanSettings.useOTAA ? "OTAA" : "ABP"));
    if (!lorawanSettings.useOTAA && lorawanSettings.devAddr > 0) { // ABP mode with preset keys
      LMIC_setSession(lorawanSettings.netid, lorawanSettings.devAddr,
         lorawanSettings.nwksKey, lorawanSettings.appsKey);
    }
  }
}


bool saveLoRaWANSession() {
  lorawanSession.lmic = LMIC;
  lorawanSession.crc = crc16((uint8_t *) &lorawanSession.lmic, sizeof(lorawanSession.lmic));
  return saveSettings(lorawanSession, EEPROM_LORAWAN_SESSION_ADDR, "LoRaWAN session");
}


bool resetLoRaWANSession() {
  Serial.println(F("Reset LoRaWAN session."));
  logMsg("reset LoRaWAN session");
  memset(&lorawanSession, 0, sizeof(lorawanSession));
  return saveSettings(lorawanSession, EEPROM_LORAWAN_SESSION_ADDR, "LoRaWAN session");
}


void loadLoRaWANSettings() {
  loraprefs_t buf;

  memset(&lorawanSettings, 0, sizeof(lorawanSettings));
  setDefaults(&lorawanSettings); // set struct with defaults values from config.h
  loadSettings(&lorawanSettings, &buf, offsetof(loraprefs_t, crc), EEPROM_LORAWAN_SETTINGS_ADDR, "LoRaWAN settings");
  printLoRaWANSettings(&lorawanSettings);
}


// save LoRaWAN settings; if LMIC is active also save current session
// keys with settings struct or if updatesession is true, update current
// session with keys and data rate from settings
bool saveLoRaWANSettings(bool updatesession) {
  if (lorawanSettings.crc) { // use an existing crc as flag for a valid settings struct
    lorawanSettings.netid = lorawanSettings.useTTN ? LORAWAN_NETID_TTN : LORAWAN_NETID;
    if (lmic_ready()) {
      if (updatesession) {
        LMIC_setDrTxpow(lorawanSettings.drSend, LORAWAN_TXPOWER);
        LMIC_setSession(lorawanSettings.netid, lorawanSettings.devAddr,
            lorawanSettings.nwksKey, lorawanSettings.appsKey);
      } else {
        LMIC_getSessionKeys(&lorawanSettings.netid, &lorawanSettings.devAddr,
            lorawanSettings.nwksKey, lorawanSettings.appsKey);
      }
    }
    lorawanSettings.crc = crc16((uint8_t *) &lorawanSettings, offsetof(loraprefs_t, crc));
    return saveSettings(lorawanSettings, EEPROM_LORAWAN_SETTINGS_ADDR, "LoRaWAN settings") && printLoRaWANSettings(&lorawanSettings);
  }
  return false;
}


bool resetLoRaWANSettings() {
  Serial.println(F("Reset LoRaWAN settings."));
  logMsg("reset LoRaWAN settings");
  memset(&lorawanSettings, 0, sizeof(lorawanSettings));
  setDefaults(&lorawanSettings);
  return saveSettings(lorawanSettings, EEPROM_LORAWAN_SETTINGS_ADDR, "LoRaWAN settings") && printLoRaWANSettings(&lorawanSettings);
}
#endif
