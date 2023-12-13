#ifndef JL_CONFIG_H
#define JL_CONFIG_H

// All of these configs MUST have a distinct number.
#define JL_CONFIG_NONE 0
#define JL_CONFIG_VEST 1
#define JL_CONFIG_GAUNTLET 2
#define JL_CONFIG_STAFF 3
#define JL_CONFIG_CAPTAIN_HAT 4
#define JL_CONFIG_ROPELIGHT 5
#define JL_CONFIG_HAMMER 6
#define JL_CONFIG_FAIRY_WAND 7

#define JL_MERGE_TOKENS_INTERNAL(a_, b_) a_##b_
#define JL_MERGE_TOKENS(a_, b_) JL_MERGE_TOKENS_INTERNAL(a_, b_)

#ifndef JL_CONFIG
#error "Preprocessor macro JL_CONFIG must be defined using compiler flags"
#endif  // JL_CONFIG

#define JL_IS_CONFIG(config_) (JL_MERGE_TOKENS(JL_CONFIG_, JL_CONFIG) == JL_MERGE_TOKENS(JL_CONFIG_, config_))

#ifndef CORE2AWS
#define CORE2AWS 0
#endif  // CORE2AWS

#define JL_ATOM_MATRIX (!CORE2AWS)

// Choose whether to enable button lock.
#ifndef BUTTON_LOCK
#define BUTTON_LOCK 1
#endif  // BUTTON_LOCK

#ifndef GLOW_ONLY
#define GLOW_ONLY 0
#endif  // GLOW_ONLY

#ifndef ATOM_MATRIX_SCREEN
#ifdef ESP32
#define ATOM_MATRIX_SCREEN 1
#else  // ESP32
#define ATOM_MATRIX_SCREEN 0
#endif  // ESP32
#endif  // ATOM_MATRIX_SCREEN

#ifndef BOOT_NAME
#define BOOT_NAME X
#endif  // BOOT_NAME

#ifndef REVISION
#define REVISION 10
#endif  // REVISION

// Extra indirection ensures preprocessor expands macros in correct order.
#define STRINGIFY_INNER(s) #s
#define STRINGIFY(s) STRINGIFY_INNER(s)

#ifndef BOOT_MESSAGE
#define BOOT_MESSAGE STRINGIFY(BOOT_NAME) "-" STRINGIFY(REVISION)
#endif  // BOOT_MESSAGE

#ifndef JL_TIMING
#define JL_TIMING 0
#endif  // JL_TIMING

#ifndef JL_INSTRUMENTATION
#define JL_INSTRUMENTATION 0
#endif  // JL_INSTRUMENTATION

#ifndef M5STAMP_PICO
#define M5STAMP_PICO 0
#endif  // M5STAMP_PICO

#ifndef M5STAMP_C3U
#define M5STAMP_C3U 0
#endif  // M5STAMP_C3U

#define JL_WIFI_SSID "FISHLIGHT"
#define JL_WIFI_PASSWORD "155155155"

#endif  // JL_CONFIG_H
