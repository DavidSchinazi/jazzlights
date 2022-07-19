#ifndef UNISPARKS_BOARD_H
#define UNISPARKS_BOARD_H

#include "unisparks/config.h"

#if WEARABLE

#include "unisparks/layout.hpp"

namespace unisparks {

#if ORANGE_VEST
#  define LEDNUM 360
#endif // ORANGE_VEST

#if CAMP_SIGN
#  define LEDNUM 900
#endif // CAMP_SIGN

#if GECKO_FOOT
#  define LEDNUM 68
#endif  // GECKO_FOOT

#if GUPPY
#  define LEDNUM 300
#endif // GUPPY

#if HAMMER
#  define LEDNUM 20
#endif // HAMMER

#if CAMP_SIGN || HAMMER || GECKO_FOOT
#  define FIRST_BRIGHTNESS MAX_BRIGHTNESS
#endif // CAMP_SIGN

const Layout* GetLayout();

}  // namespace unisparks

#endif  // WEARABLE

#endif // UNISPARKS_BOARD_H
