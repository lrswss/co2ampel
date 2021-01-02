/***************************************************************************
  Copyright (c) 2020-2021 Lars Wessels

  This file a part of the "CO2-Ampel" source code.
  https://github.com/lrswss/co2ampel

  Published under Apache License 2.0

***************************************************************************/

#ifndef _LED_H
#define _LED_H

#define NEOPIXEL_DATA_PIN 0  // GPIO0
#define FASTLED_INTERNAL

#include <Arduino.h>
#include <FastLED.h>
#include "config.h"

#define GREEN CRGB::Green
#define RED CRGB::Red
#define BLUE CRGB::Blue
#define YELLOW CRGB::Yellow
#define MAGENTA CRGB::Magenta
#define WHITE CRGB::White
#define CYAN CRGB::Cyan
#define ORANGE CRGB::DarkOrange

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

void led_init();
void clear_leds(PixelBits pixels);
void save_leds();
void restore_leds();
void set_leds(PixelBits pixels, uint32_t color);
void blink_leds(PixelBits pixels, uint32_t color, uint16_t pause, uint8_t blinks, bool restore);
void timer_leds(PixelBits pixels, uint32_t color, uint16_t timeout_ms, bool restore);
void toggle_leds(PixelBits pixels, uint32_t color);
uint8_t leds_on();

#endif
