/***************************************************************************
  Copyright (c) 2020 Lars Wessels

  This file a part of the "CO2-Ampel" source code.
  https://github.com/lrswss/co2ampel

  Published under Apache License 2.0

***************************************************************************/

#include <Arduino.h>
#include "led.h"

Adafruit_NeoPixel neopixels = Adafruit_NeoPixel(NUM_PIXELS, 0, NEO_GRB + NEO_KHZ800);
static uint32_t currentState[NUM_PIXELS];
static uint32_t savedState[NUM_PIXELS];


// setup WS2812 LEDs (all off)
void led_init() {
  Serial.printf("Setup %d WS2812 LEDs...\n", NUM_PIXELS);
  neopixels.begin();
  clear_leds(ALL_LEDS);
  blink_leds(SYSTEM_LEDS, GREEN, 50, 2, false);
}


// turn off given pixels
void clear_leds(PixelBits pixels) {
  set_leds(pixels, 0);
}


// save current LED state
void save_leds() {
  memcpy(savedState, currentState, sizeof(savedState));
}


// restore previous LED state
void restore_leds() {
  for (uint8_t i = 0; i < NUM_PIXELS; i++) {
    neopixels.setPixelColor(i, savedState[i]);
  }
  neopixels.show();
}


// set selected LEDs to given color
// clear alle rings pixel before setting subset of them
void set_leds(PixelBits pixels, uint32_t color) {
  uint16_t pixel = 1; 

  for (uint8_t i = 0; i < NUM_PIXELS; i++) {
    if ((pixels & QUARTER_RING || pixels & HALF_RING) && (pixel & FULL_RING)) {
      currentState[i] = 0;
      neopixels.setPixelColor(i, 0);
    }
    if (pixel & pixels) {
      currentState[i] = color;
      neopixels.setPixelColor(i, color);
    }
    pixel = pixel << 1;    
  } 
  neopixels.show();
}


// set selected LEDs to given color with given timeout in ms
// optionally restore previous LED state
void timer_leds(PixelBits pixels, uint32_t color, uint16_t timeout_ms, bool restore) {
  set_leds(pixels, color);
  delay(timeout_ms);
  if (restore)
    restore_leds();
  else
    clear_leds(pixels);
}


void toogle_leds(PixelBits pixels, uint32_t color) {
  static bool ledOn = false;
  if (!ledOn) {
    set_leds(pixels, color);
    ledOn = true;
  } else {
    clear_leds(pixels);
    ledOn = false;
  }
}


// blink selected WS2812 neopixels, optionally restore previous LED state
void blink_leds(PixelBits pixels, uint32_t color, uint16_t pause, uint8_t blinks, bool restore) {
  for (uint8_t i = 0; i < blinks; i++) {
    set_leds(pixels, color);
    neopixels.show();
    delay(pause);
    clear_leds(pixels);
    neopixels.show();
    delay(pause);
  }
  if (restore)
    restore_leds();
}
