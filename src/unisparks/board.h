#ifndef UNISPARKS_BOARD_H
#define UNISPARKS_BOARD_H

#include "unisparks/config.h"

#if WEARABLE

#include "unisparks/layout.hpp"

namespace unisparks {

#ifndef ORANGE_VEST
#  define ORANGE_VEST 0
#endif // ORANGE_VEST
#ifndef CAMP_SIGN
#  define CAMP_SIGN 0
#endif // CAMP_SIGN
#ifndef GUPPY
#  define GUPPY 0
#endif // GUPPY
#ifndef HAMMER
#  define HAMMER 0
#endif // HAMMER

#if ORANGE_VEST
#  define LEDNUM 360
#endif // ORANGE_VEST

#if CAMP_SIGN
#  define LEDNUM 900
#endif // CAMP_SIGN

#if GUPPY
#  define LEDNUM 300
#endif // GUPPY

#if HAMMER
#  define LEDNUM 20
#endif // HAMMER

#if CAMP_SIGN || HAMMER
#  define FIRST_BRIGHTNESS MAX_BRIGHTNESS
#endif // CAMP_SIGN

const Layout* GetLayout();

}  // namespace unisparks

#endif  // WEARABLE

#endif // UNISPARKS_BOARD_H
