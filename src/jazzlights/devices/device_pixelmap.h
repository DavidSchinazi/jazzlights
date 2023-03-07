#ifndef JL_BOARD_H
#define JL_BOARD_H

#include "jazzlights/config.h"

#if RENDERABLE

#include "jazzlights/layouts/layout.h"

namespace jazzlights {

#if ORANGE_VEST
#define LEDNUM 360
#endif  // ORANGE_VEST

#if IS_GAUNTLET
#define LEDNUM 300
#endif  // IS_GAUNTLET

#if CAMP_SIGN
#define LEDNUM 900
#endif  // CAMP_SIGN

#if GECKO_FOOT
#define LEDNUM 102
#endif  // GECKO_FOOT

#if IS_CABOOSE_WALL
#define LEDNUM 206
#endif  // IS_CABOOSE_WALL

#if IS_ROBOT
#define LEDNUM 310
#define LEDNUM2 68
#endif  // IS_STAFF

#if IS_GUPPY
#define LEDNUM 300
#endif  // IS_GUPPY

#if HAMMER
#define LEDNUM 20
#endif  // HAMMER

#if FAIRY_WAND
#define LEDNUM 9
#endif  // FAIRY_WAND

#if IS_STAFF
#define LEDNUM 36
#define LEDNUM2 33
#endif  // IS_STAFF

#if IS_CAPTAIN_HAT
#define LEDNUM 60
#endif  // IS_CAPTAIN_HAT

#if IS_ROPELIGHT
#define LEDNUM 300
#endif  // IS_ROPELIGHT

#if CAMP_SIGN || HAMMER || GECKO_FOOT || IS_ROBOT
#define FIRST_BRIGHTNESS MAX_BRIGHTNESS
#endif  // CAMP_SIGN

const Layout* GetLayout();

#ifndef LEDNUM2
#define LEDNUM2 0
#endif  // LEDNUM2

#if LEDNUM2
const Layout* GetLayout2();
#endif  // LEDNUM2

}  // namespace jazzlights

#endif  // RENDERABLE

#endif  // JL_BOARD_H
