#ifndef JL_CONFIG_H
#define JL_CONFIG_H

// Misc macros.
#define JL_MERGE_TOKENS_INTERNAL(a_, b_) a_##b_
#define JL_MERGE_TOKENS(a_, b_) JL_MERGE_TOKENS_INTERNAL(a_, b_)

// Configurations.
#ifndef JL_CONFIG
#error "Preprocessor macro JL_CONFIG must be defined using compiler flags"
#endif  // JL_CONFIG

// All of these configs MUST have a distinct number.
#define JL_CONFIG_NONE 0
#define JL_CONFIG_VEST 1
#define JL_CONFIG_GAUNTLET 2
#define JL_CONFIG_STAFF 3
#define JL_CONFIG_HAT 4
#define JL_CONFIG_ROPELIGHT 5
#define JL_CONFIG_HAMMER 6
#define JL_CONFIG_FAIRY_WAND 7
#define JL_CONFIG_CLOUDS 8
#define JL_CONFIG_SHOE 9

#define JL_IS_CONFIG(config_) (JL_MERGE_TOKENS(JL_CONFIG_, JL_CONFIG) == JL_MERGE_TOKENS(JL_CONFIG_, config_))

// Controllers.
#ifndef JL_CONTROLLER
#error "Preprocessor macro JL_CONTROLLER must be defined using compiler flags"
#endif  // JL_CONTROLLER

#define JL_CONTROLLER_NATIVE 0
#define JL_CONTROLLER_ATOM_MATRIX 1
#define JL_CONTROLLER_ATOM_LITE 2
#define JL_CONTROLLER_CORE2AWS 3
#define JL_CONTROLLER_M5STAMP_PICO 4
#define JL_CONTROLLER_M5STAMP_C3U 5
#define JL_CONTROLLER_ATOM_S3 6

#define JL_IS_CONTROLLER(controller_) \
  (JL_MERGE_TOKENS(JL_CONTROLLER_, JL_CONTROLLER) == JL_MERGE_TOKENS(JL_CONTROLLER_, controller_))

#ifndef JL_DEV
#define JL_DEV 0
#endif  // JL_DEV

#ifndef BOOT_NAME
#define BOOT_NAME X
#endif  // BOOT_NAME

#ifndef REVISION
#define REVISION 11
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

#ifndef JL_BOUNDS_CHECKS
#ifdef ARDUINO
#ifdef PIO_UNIT_TESTING
#define JL_BOUNDS_CHECKS 1
#else  // PIO_UNIT_TESTING
#define JL_BOUNDS_CHECKS 0
#endif  // PIO_UNIT_TESTING
#else   // ARDUINO
#define JL_BOUNDS_CHECKS 1
#endif  // ARDUINO
#endif  // JL_BOUNDS_CHECKS

#define JL_WIFI_SSID "JazzLights"
#define JL_WIFI_PASSWORD "burningblink"

#endif  // JL_CONFIG_H
