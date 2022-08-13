// Arduino API wrapper
#ifndef UNISPARKS_H
#define UNISPARKS_H

#include "unisparks/config.h"

#if WEARABLE

// Fixes flickering <https://github.com/FastLED/FastLED/issues/306>.
#define FASTLED_ALLOW_INTERRUPTS 0

// Silences FastLED pragmas <https://github.com/FastLED/FastLED/issues/363>.
#define FASTLED_INTERNAL 1

#ifdef ESP8266
// Required to map feather huzzah and LoLin nodecmu pins properly.
#  define FASTLED_ESP8266_RAW_PIN_ORDER
#endif // ESP8266

#include <FastLED.h>

#endif // WEARABLE

#include "unisparks/board.h"
#include "unisparks/types.h"
#include "unisparks/layouts/matrix.hpp"
#include "unisparks/layouts/pixelmap.hpp"
#include "unisparks/networks/esp32_ble.hpp"
#include "unisparks/networks/esp8266wifi.hpp"
#include "unisparks/networks/arduinoethernet.hpp"
#include "unisparks/player.hpp"
#include "unisparks/util/log.hpp"
#include "unisparks/core2.h"
#include "unisparks/vest.h"
#include "unisparks/text.h"
#include "unisparks/button.h"

namespace Unisparks = unisparks;

#endif /* UNISPARKS_H */
