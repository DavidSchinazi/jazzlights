// Arduino API wrapper
#ifndef JAZZLIGHTS_H
#define JAZZLIGHTS_H

#include "jazzlights/config.h"

#if WEARABLE

// Fixes flickering <https://github.com/FastLED/FastLED/issues/306>.
#define FASTLED_ALLOW_INTERRUPTS 0

// Silences FastLED pragmas <https://github.com/FastLED/FastLED/issues/363>.
#define FASTLED_INTERNAL 1

#ifdef ESP8266
// Required to map feather huzzah and LoLin nodecmu pins properly.
#define FASTLED_ESP8266_RAW_PIN_ORDER
#endif  // ESP8266

#include <FastLED.h>

#endif  // WEARABLE

#include "jazzlights/text.h"
#include "jazzlights/types.h"
#include "jazzlights/util/log.h"

#endif  // JAZZLIGHTS_H
