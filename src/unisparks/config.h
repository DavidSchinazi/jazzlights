#ifndef UNISPARKS_CONFIG_H
#define UNISPARKS_CONFIG_H

#ifndef WEARABLE
#  define WEARABLE 0
#endif // WEARABLE

// Pick which vest to build here.
#if !defined(CAMP_SIGN) && !defined(ROPELIGHT) && !defined(ORANGE_VEST) && !defined(HAMMER) && !defined(GUPPY)
#  define ORANGE_VEST WEARABLE
// #define CAMP_SIGN 1
// #define GUPPY 1
#endif

#ifndef CAMP_SIGN
#  define CAMP_SIGN 0
#endif // CAMP_SIGN

#ifndef ROPELIGHT
#  define ROPELIGHT 0
#endif // ROPELIGHT

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

#ifndef BOOT_NAME
#  define BOOT_NAME X
#endif // BOOT_NAME

#ifndef REVISION
#  define REVISION 5
#endif // REVISION

// Extra indirection ensures preprocessor expands macros in correct order.
#define STRINGIFY_INNER(s) #s
#define STRINGIFY(s) STRINGIFY_INNER(s)

#ifndef BOOT_MESSAGE
#  define BOOT_MESSAGE STRINGIFY(BOOT_NAME) "-" STRINGIFY(REVISION)
#endif // BOOT_MESSAGE

#endif // UNISPARKS_CONFIG_H
