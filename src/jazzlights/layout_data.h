#ifndef JL_LAYOUT_DATA_H
#define JL_LAYOUT_DATA_H

#include "jazzlights/config.h"

#ifdef ESP32

#define JL_LENGTH(_a) (sizeof(_a) / sizeof((_a)[0]))

#include "jazzlights/fastled_runner.h"

#if JL_IS_CONTROLLER(CORE2AWS) || JL_IS_CONTROLLER(M5STAMP_PICO)
#define LED_PIN 32
#elif JL_IS_CONTROLLER(M5STAMP_C3U)
#define LED_PIN 1
#elif JL_IS_CONTROLLER(ATOM_MATRIX) || JL_IS_CONTROLLER(ATOM_LITE)
#define LED_PIN 26
#define LED_PIN2 32
#elif JL_IS_CONTROLLER(ATOM_S3)
#define LED_PIN 2
#define LED_PIN2 1
#else
#error "Unexpected controller"
#endif

namespace jazzlights {

void AddLedsToRunner(FastLedRunner* runner);

}  // namespace jazzlights

#endif  // ESP32

#endif  // JL_LAYOUT_DATA_H
