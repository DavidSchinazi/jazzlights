#ifndef JL_LAYOUT_LAYOUT_DATA_H
#define JL_LAYOUT_LAYOUT_DATA_H

#include "jazzlights/config.h"

#ifdef ESP32

#define JL_LENGTH(_a) (sizeof(_a) / sizeof((_a)[0]))

#include "jazzlights/fastled_runner.h"

#if !JL_IS_CONFIG(PHONE)
#if JL_IS_CONTROLLER(CORE2AWS) || JL_IS_CONTROLLER(M5STAMP_PICO)
#define LED_PIN 32
#elif JL_IS_CONTROLLER(M5STAMP_C3U)
#define LED_PIN 1
#elif JL_IS_CONTROLLER(ATOM_MATRIX) || JL_IS_CONTROLLER(ATOM_LITE)
#if JL_IS_CONFIG(ORRERY)
#define LED_PIN 22
#define LED_PIN2 19
#else  // ORRERY
#define LED_PIN 26
#define LED_PIN2 32
#endif  // ORRERY
#elif JL_IS_CONTROLLER(ATOM_S3) || JL_IS_CONTROLLER(ATOM_S3_LITE) || JL_IS_CONTROLLER(M5_NANO_C6)
#if JL_IS_CONFIG(ORRERY)
#define LED_PIN 5
#define LED_PIN2 6
#else  // ORRERY
#define LED_PIN 2
#define LED_PIN2 1
#endif  // ORRERY
#elif JL_IS_CONTROLLER(M5STAMP_S3)
#define LED_PIN 13
#define LED_PIN2 15
#else
#error "Unexpected controller"
#endif
#endif  // PHONE

namespace jazzlights {

void AddLedsToRunner(FastLedRunner* runner);

}  // namespace jazzlights

#endif  // ESP32

#endif  // JL_LAYOUT_LAYOUT_DATA_H
