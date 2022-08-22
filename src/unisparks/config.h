#ifndef UNISPARKS_CONFIG_H
#define UNISPARKS_CONFIG_H

#ifndef WEARABLE
#  define WEARABLE 0
#endif // WEARABLE

// Pick which vest to build here.
#if !defined(CAMP_SIGN) && !defined(IS_ROPELIGHT) && !defined(ORANGE_VEST) && \
    !defined(HAMMER) && !defined(IS_GUPPY) && !defined(GECKO_FOOT) && \
    !defined(FAIRY_WAND) && !defined(IS_STAFF) && !defined(IS_CAPTAIN_HAT) && \
    !defined(IS_GAUNTLET)
#  define ORANGE_VEST WEARABLE
#endif

#ifndef ORANGE_VEST
#  define ORANGE_VEST 0
#endif // ORANGE_VEST

#ifndef CORE2AWS
#  define CORE2AWS 0
#endif  // CORE2AWS

#ifndef IS_GAUNTLET
#  define IS_GAUNTLET 0
#endif  // IS_GAUNTLET

#ifndef GECKO_FOOT
#  define GECKO_FOOT 0
#endif  // GECKO_FOOT

#ifndef CAMP_SIGN
#  define CAMP_SIGN 0
#endif // CAMP_SIGN

#ifndef IS_GUPPY
#  define IS_GUPPY 0
#endif // IS_GUPPY

#ifndef HAMMER
#  define HAMMER 0
#endif // HAMMER

#ifndef FAIRY_WAND
#  define FAIRY_WAND 0
#endif // FAIRY_WAND

#ifndef IS_ROPELIGHT
#  define IS_ROPELIGHT 0
#endif // IS_ROPELIGHT

// Choose whether to enable button lock.
#ifndef BUTTON_LOCK
#  define BUTTON_LOCK 1
#endif // BUTTON_LOCK

#ifndef GLOW_ONLY
#  define GLOW_ONLY 0
#endif // GLOW_ONLY

#ifndef ATOM_MATRIX_SCREEN
#  ifdef ESP32
#    define ATOM_MATRIX_SCREEN 1
#  else // ESP32
#    define ATOM_MATRIX_SCREEN 0
#  endif // ESP32
#endif // ATOM_MATRIX_SCREEN

#ifndef GECKO_SCALES
#  define GECKO_SCALES 0
#endif  // GECKO_SCALES

#ifndef IS_STAFF
#  define IS_STAFF 0
#endif  // IS_STAFF

#ifndef IS_CAPTAIN_HAT
#  define IS_CAPTAIN_HAT 0
#endif  // IS_CAPTAIN_HAT

#ifndef BOOT_NAME
#  define BOOT_NAME X
#endif // BOOT_NAME

#ifndef REVISION
#  define REVISION 7
#endif // REVISION

// Extra indirection ensures preprocessor expands macros in correct order.
#define STRINGIFY_INNER(s) #s
#define STRINGIFY(s) STRINGIFY_INNER(s)

#ifndef BOOT_MESSAGE
#  define BOOT_MESSAGE STRINGIFY(BOOT_NAME) "-" STRINGIFY(REVISION)
#endif // BOOT_MESSAGE

#endif // UNISPARKS_CONFIG_H
