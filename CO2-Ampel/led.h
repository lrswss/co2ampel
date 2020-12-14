/***************************************************************************
  Copyright (c) 2020 Lars Wessels

  This file a part of the "CO2-Ampel" source code.
  https://github.com/lrswss/co2ampel

  Published under Apache License 2.0

***************************************************************************/

#ifndef _LED_H
#define _LED_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

#define GREEN Adafruit_NeoPixel::Color(0,255,0)
#define RED Adafruit_NeoPixel::Color(255,0,0)
#define BLUE Adafruit_NeoPixel::Color(0,0,255)
#define YELLOW Adafruit_NeoPixel::Color(255,110,0)
#define MAGENTA Adafruit_NeoPixel::Color(255,0,255)
#define WHITE Adafruit_NeoPixel::Color(255,150,150)
#define PURPLE Adafruit_NeoPixel::Color(150,0,200)
#define CYAN Adafruit_NeoPixel::Color(0,255,255)

#ifdef HAS_LORAWAN_SHIELD
// two WS2812 LEDs on LoRaWAN-Shield and ring with 12 LEDs
#define NUM_PIXELS 14
enum PixelBits {
  ALL_LEDS = 0x3FFF,
  SYSTEM_LED1 = 0x0001,
  SYSTEM_LED2 = 0x0002,
  SYSTEM_LEDS = 0x0003,
  QUARTER_RING = 0x2220,
  HALF_RING = 0x1554,
  FULL_RING = 0x3FFC  
};
#else
// only ring with 12 WS2812 present
#define NUM_PIXELS 12
enum PixelBits {
  ALL_LEDS = 0x0FFF,
  SYSTEM_LED1 = 0x0001,
  SYSTEM_LED2 = 0x0002,
  SYSTEM_LEDS = 0x0003,
  QUARTER_RING = 0x0444,
  HALF_RING = 0x0554,
  FULL_RING = 0x0FFC 
};
#endif

extern Adafruit_NeoPixel neopixels;

void led_init();
void clear_leds(PixelBits pixels);
void save_leds();
void restore_leds();
void set_leds(PixelBits pixels, uint32_t color);
void blink_leds(PixelBits pixels, uint32_t color, uint16_t pause, uint8_t blinks, bool restore);
void timer_leds(PixelBits pixels, uint32_t color, uint16_t timeout_ms, bool restore);
void toogle_leds(PixelBits pixels, uint32_t color);

#endif
