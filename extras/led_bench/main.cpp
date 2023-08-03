#include "jazzlights/config.h"

// This file hasn't been designed with a build file yet.
// To use it, copy it to src/main.cpp and run on ESP32.

// Results on M5Atom for WS2812:
// This relies on the RMT controller in the M5Atom to asynchronously communicate
// with all LED strands at once so that sending to 1 or 8 strands takes the same
// amount of time. (The RMT controller on that device only supports 8 channels
// at once). For 8 separate lines of n LEDs each:
// 360 LEDs = 23 ms =  43 fps
// 310 LEDs = 19 ms =  52 fps
// 250 LEDs = 15 ms =  66 fps
// 216 LEDs = 14 ms =  71 fps
// 200 LEDs = 12 ms =  83 fps
// 150 LEDs = 10 ms = 100 fps

#ifdef ARDUINO

#include <Arduino.h>

#include "jazzlights/vest.h"

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

//#define ADD_LEDS(n, pin) c##n = &FastLED.addLeds<WS2812, /*DATA_PIN=*/pin, GRB>(leds##n, numLeds##n);
#define ADD_LEDS(n, pin)                                                                                    \
  c##n = &FastLED.addLeds<WS2801, /*DATA_PIN=*/pin, /*CLOCK_PIN=*/32, GRB, /*SPI_SPEED=*/DATA_RATE_MHZ(1)>( \
      leds##n, numLeds##n);

#define LOG_TIME(s)                           \
  milli2 = millis();                          \
  Serial.printf(#s ": %d\n", milli2 - milli); \
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
  Serial.begin(115200);
  ADD_LEDS(1, 27)
  ADD_LEDS(2, 26)
  ADD_LEDS(3, 21)
  ADD_LEDS(4, 25)
  ADD_LEDS(5, 19)
  ADD_LEDS(6, 22)
  ADD_LEDS(7, 23)
  ADD_LEDS(8, 33)
  // ADD_LEDS(9, 32)
  // jazzlights::vestSetup();
  uint32_t milli = millis(), milli2;
  SHOW_LEDS(1)
  SHOW_LEDS(2)
  SHOW_LEDS(3)
  SHOW_LEDS(4)
  SHOW_LEDS(5)
  SHOW_LEDS(6)
  SHOW_LEDS(7)
  SHOW_LEDS(8)
}

void loop() {
  Serial.println();
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
  /*
  SHOW_LEDS(1)
  */
  SHOW_LEDS(2)
  /*
  SHOW_LEDS(3)
  SHOW_LEDS(4)
  SHOW_LEDS(5)
  SHOW_LEDS(6)
  SHOW_LEDS(7)
  */
  SHOW_LEDS(8)
  // SHOW_LEDS(9)
  // jazzlights::vestLoop();
}

#endif  // ARDUINO
