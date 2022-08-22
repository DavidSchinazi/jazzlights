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
#  define LEDNUM 102
#endif  // GECKO_FOOT

#if IS_GUPPY
#  define LEDNUM 300
#endif // IS_GUPPY

#if HAMMER
#  define LEDNUM 20
#endif // HAMMER

#if FAIRY_WAND
#  define LEDNUM 9
#endif // FAIRY_WAND

#if IS_STAFF
#  define LEDNUM 36
#  define LEDNUM2 33
#endif  // IS_STAFF

#if IS_CAPTAIN_HAT
#  define LEDNUM 60
#endif  // IS_CAPTAIN_HAT

#if IS_ROPELIGHT
#  define LEDNUM 300
#endif  // IS_ROPELIGHT

#if CAMP_SIGN || HAMMER || GECKO_FOOT
#  define FIRST_BRIGHTNESS MAX_BRIGHTNESS
#endif // CAMP_SIGN

const Layout* GetLayout();

#ifndef LEDNUM2
#  define LEDNUM2 0
#endif  // LEDNUM2

#if LEDNUM2
const Layout* GetLayout2();
#endif  // LEDNUM2

}  // namespace unisparks

#endif  // WEARABLE

#endif // UNISPARKS_BOARD_H
