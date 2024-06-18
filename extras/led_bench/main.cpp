#include "jazzlights/config.h"

// This file hasn't been designed with a build file yet.
// To use it, copy it to src/main.cpp and run on ESP32.

// This relies on the RMT controller in the ESP32 to asynchronously communicate with all LED strands at once so that
// sending to 1 or 8 strands takes the same amount of time. (The RMT controller on that device only supports 8 channels
// at once). Actually it appears that FASTLED_RMT_MAX_CHANNELS might be 4. So the results are 1-4 vs 5-8 strands.

// M5AtomMatrix - WS2812B or 1-4 separate lines of n LEDs each:
// 360 LEDs = 11 ms =  89 fps
// 310 LEDs = 10 ms = 100 fps
// 250 LEDs =  8 ms = 125 fps
// 216 LEDs =  7 ms = 142 fps
// 200 LEDs =  6 ms = 166 fps
// 150 LEDs =  5 ms = 200 fps

// M5AtomMatrix - WS2812B for 5-8 separate lines of n LEDs each:
// 360 LEDs = 23 ms =  43 fps
// 310 LEDs = 19 ms =  52 fps
// 250 LEDs = 15 ms =  66 fps
// 216 LEDs = 14 ms =  71 fps
// 200 LEDs = 12 ms =  83 fps
// 150 LEDs = 10 ms = 100 fps

#ifndef PIO_UNIT_TESTING

#ifdef ARDUINO

#include <Arduino.h>

#include "jazzlights/fastled_wrapper.h"

#define ALLOCATE_LED(n, num)         \
  constexpr size_t numLeds##n = num; \
  CLEDController* c##n = nullptr;    \
  CRGB leds##n[numLeds##n] = {};

uint8_t brightness = 10;

void setLEDs(uint32_t milli, CRGB* leds, size_t numLeds, CRGB colorA, CRGB colorB) {
  for (size_t i = 0; i < numLeds; i++) {
    if (((i % 2) == 0) ^ (milli % 1000 < 500)) {
      leds[i] = colorA;
    } else {
      leds[i] = colorB;
    }
  }
}

#if 1
#define ADD_LEDS(n, pin) c##n = &FastLED.addLeds<WS2812B, /*DATA_PIN=*/pin, GRB>(leds##n, numLeds##n);
#else
#define ADD_LEDS(n, pin)                                                                                    \
  c##n = &FastLED.addLeds<WS2801, /*DATA_PIN=*/pin, /*CLOCK_PIN=*/32, GRB, /*SPI_SPEED=*/DATA_RATE_MHZ(1)>( \
      leds##n, numLeds##n);
#endif

#define LOG_TIME(s)                      \
  milli2 = millis();                     \
  ::printf(#s ": %d\n", milli2 - milli); \
  milli = milli2;

#define SET_LEDS(n, ca, cb) setLEDs(milli, leds##n, numLeds##n, ca, cb);
#define SHOW_LEDS(n)          \
  c##n->showLeds(brightness); \
  LOG_TIME(show##n)

#define LED_SIZE 360

ALLOCATE_LED(1, LED_SIZE)
ALLOCATE_LED(2, LED_SIZE)
ALLOCATE_LED(3, LED_SIZE)
ALLOCATE_LED(4, LED_SIZE)
ALLOCATE_LED(5, LED_SIZE)
ALLOCATE_LED(6, LED_SIZE)
ALLOCATE_LED(7, LED_SIZE)
ALLOCATE_LED(8, LED_SIZE)
// ALLOCATE_LED(9, 200)

void setup() {
  ADD_LEDS(1, 27)
  ADD_LEDS(2, 26)
  ADD_LEDS(3, 21)
  ADD_LEDS(4, 25)
  //   ADD_LEDS(5, 19)
  //   ADD_LEDS(6, 22)
  //   ADD_LEDS(7, 23)
  //   ADD_LEDS(8, 33)
  //   ADD_LEDS(9, 32)
  uint32_t milli = millis(), milli2;
  SHOW_LEDS(1)
  SHOW_LEDS(2)
  SHOW_LEDS(3)
  SHOW_LEDS(4)
  //   SHOW_LEDS(5)
  //   SHOW_LEDS(6)
  //   SHOW_LEDS(7)
  //   SHOW_LEDS(8)
}

void loop() {
  uint32_t milli = millis(), milli2;
  SET_LEDS(1, CRGB::Red, CRGB::Green)
  SET_LEDS(2, CRGB::Yellow, CRGB::Blue)
  SET_LEDS(3, CRGB::White, CRGB::Purple)
  SET_LEDS(4, CRGB::Orange, CRGB::Teal)
  SET_LEDS(5, CRGB::Orange, CRGB::Red)
  SET_LEDS(6, CRGB::Orange, CRGB::Red)
  SET_LEDS(7, CRGB::Orange, CRGB::Red)
  SET_LEDS(8, CRGB::Orange, CRGB::Red)
  // SET_LEDS(9, CRGB::Orange, CRGB::Red)
  LOG_TIME(setld)

  SHOW_LEDS(1)
  SHOW_LEDS(2)
  SHOW_LEDS(3)
  SHOW_LEDS(4)
  //   SHOW_LEDS(5)
  //   SHOW_LEDS(6)
  //   SHOW_LEDS(7)
  //   SHOW_LEDS(8)
  //   SHOW_LEDS(9)
}

#endif  // ARDUINO

#endif  // PIO_UNIT_TESTING
