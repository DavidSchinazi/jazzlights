#ifndef JAZZLIGHTS_BOARD_H
#define JAZZLIGHTS_BOARD_H

#include "jazzlights/config.h"

#ifdef ARDUINO

#include "jazzlights/layout.h"

namespace jazzlights {

#if ORANGE_VEST
#define LEDNUM 360
#endif  // ORANGE_VEST

#if IS_GAUNTLET
#define LEDNUM 300
#endif  // IS_GAUNTLET

#if HAMMER
#define LEDNUM 20
#endif  // HAMMER

#if FAIRY_WAND
#define LEDNUM 9
#endif  // FAIRY_WAND

#if IS_STAFF
#define LEDNUM 36
#define LEDNUM2 33
#define BRIGHTER2 1
#endif  // IS_STAFF

#if IS_CAPTAIN_HAT
#define LEDNUM 60
#endif  // IS_CAPTAIN_HAT

#if IS_ROPELIGHT
#define LEDNUM 300
#endif  // IS_ROPELIGHT

#if HAMMER
#define FIRST_BRIGHTNESS MAX_BRIGHTNESS
#endif  // HAMMER

const Layout* GetLayout();

#ifndef LEDNUM2
#define LEDNUM2 0
#endif  // LEDNUM2

#ifndef BRIGHTER2
#define BRIGHTER2 0
#endif  // BRIGHTER2

#if LEDNUM2
const Layout* GetLayout2();
#endif  // LEDNUM2

}  // namespace jazzlights

#endif  // ARDUINO

#endif  // JAZZLIGHTS_BOARD_H
