#ifndef JL_UTIL_CONFIG_H
#define JL_UTIL_CONFIG_H

#ifdef ESP32
#include "sdkconfig.h"
#endif  // ESP32

// Misc macros.
#define JL_MERGE_TOKENS_INTERNAL(a_, b_) a_##b_
#define JL_MERGE_TOKENS(a_, b_) JL_MERGE_TOKENS_INTERNAL(a_, b_)

#if defined(JL_CUSTOM_CONFIG) && JL_CUSTOM_CONFIG
#define JL_CONFIG VEST
#define BOOT_NAME CUSTOM
#endif  // JL_CUSTOM_CONFIG

// Configurations.
// The WLED usermod can be added to any WLED build, which doesn't describe the device the JazzLights way. None of the
// code it uses depends on the device, other than special cases for creatures, orrery and clouds, so default to none
// there.
#if !defined(JL_CONFIG) && defined(JL_WLED) && JL_WLED
#define JL_CONFIG NONE
#endif  // !JL_CONFIG && JL_WLED
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
#define JL_CONFIG_PHONE 10
#define JL_CONFIG_XMAS_TREE 11
#define JL_CONFIG_CREATURE 12
#define JL_CONFIG_FAIRY_STRING 13
#define JL_CONFIG_ORRERY_LEADER 14
#define JL_CONFIG_ORRERY_PLANET 15
#define JL_CONFIG_NEW_HAT 16
#define JL_CONFIG_BOW 17
#define JL_CONFIG_RHINO_HAT 18
#define JL_CONFIG_RHINO_STAFF 19

#define JL_IS_CONFIG(config_) (JL_MERGE_TOKENS(JL_CONFIG_, JL_CONFIG) == JL_MERGE_TOKENS(JL_CONFIG_, config_))

// Controllers.
// The WLED usermod doesn't use the controller, which only picks GROVE pins here, see JL_CONFIG above.
#if !defined(JL_CONTROLLER) && defined(JL_WLED) && JL_WLED
#define JL_CONTROLLER NATIVE
#endif  // !JL_CONTROLLER && JL_WLED
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
#define JL_CONTROLLER_ATOM_S3_LITE 7
#define JL_CONTROLLER_M5STAMP_S3 8
#define JL_CONTROLLER_M5_NANO_C6 9
#define JL_CONTROLLER_CORES3 10
#define JL_CONTROLLER_M5STICK_C 11

#define JL_IS_CONTROLLER(controller_) \
  (JL_MERGE_TOKENS(JL_CONTROLLER_, JL_CONTROLLER) == JL_MERGE_TOKENS(JL_CONTROLLER_, controller_))

#if defined(CONFIG_IDF_TARGET_ESP32S3) && CONFIG_IDF_TARGET_ESP32S3
#define JL_ESP32S3 1
#else  // CONFIG_IDF_TARGET_ESP32S3
#define JL_ESP32S3 0
#endif  // CONFIG_IDF_TARGET_ESP32S3

#if defined(CONFIG_IDF_TARGET_ESP32C3) && CONFIG_IDF_TARGET_ESP32C3
#define JL_ESP32C3 1
#else  // CONFIG_IDF_TARGET_ESP32C3
#define JL_ESP32C3 0
#endif  // CONFIG_IDF_TARGET_ESP32C3

#if defined(CONFIG_IDF_TARGET_ESP32C6) && CONFIG_IDF_TARGET_ESP32C6
#define JL_ESP32C6 1
#else  // CONFIG_IDF_TARGET_ESP32C3
#define JL_ESP32C6 0
#endif  // CONFIG_IDF_TARGET_ESP32C3

#ifndef JL_ARDUINO
// Whether we're building with the Arduino core, either via `framework = arduino` or `framework = espidf, arduino`.
#ifdef ARDUINO
#define JL_ARDUINO 1
#else  // ARDUINO
#define JL_ARDUINO 0
#endif  // ARDUINO
#endif  // JL_ARDUINO

#ifndef JL_DEV
#define JL_DEV 0
#endif  // JL_DEV

#ifndef JL_DEBUG
#define JL_DEBUG 0
#endif  // JL_DEBUG

#ifndef BOOT_NAME
#define BOOT_NAME X
#endif  // BOOT_NAME

#ifndef REVISION
#define REVISION 15
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

#ifndef JL_SLOW_SCOPE_LOGS
// Logs any operation wrapped in JL_LOG_IF_SLOW() that takes longer than 20ms. Useful to find what blocks the primary
// runloop.
#define JL_SLOW_SCOPE_LOGS 0
#endif  // JL_SLOW_SCOPE_LOGS

#ifndef JL_BOUNDS_CHECKS
#ifdef ESP32
#ifdef PIO_UNIT_TESTING
#define JL_BOUNDS_CHECKS 1
#else  // PIO_UNIT_TESTING
#define JL_BOUNDS_CHECKS 0
#endif  // PIO_UNIT_TESTING
#else   // ESP32
#define JL_BOUNDS_CHECKS 1
#endif  // ESP32
#endif  // JL_BOUNDS_CHECKS

#ifndef JL_WIFI
#ifdef ESP32
#define JL_WIFI 1
#else  // ESP32
#define JL_WIFI 0
#endif  // ESP32
#endif  // JL_WIFI

#ifndef JL_ESP32_WIFI
// Esp32WiFiNetwork is currently work-in-progress.
// TODO: finish this and use it to replace ArduinoEspWiFiNetwork.
#define JL_ESP32_WIFI 0
#endif  // JL_ESP32_WIFI

#ifndef JL_ESP32_WIFI_LINK
// Use Esp32WiFiLink and Esp32SocketManager instead of Esp32WiFiNetwork. This is currently work-in-progress.
#define JL_ESP32_WIFI_LINK 0
#endif  // JL_ESP32_WIFI_LINK

#ifndef JL_ESPHOME
// Set when building as an ESPHome external component (see extras/esphome). ESPHome then manages the Wi-Fi link and
// runs the primary runloop from its own loop.
#define JL_ESPHOME 0
#endif  // JL_ESPHOME

#ifndef JL_WLED
// Set when building as a WLED usermod (see extras/wled). WLED then manages Wi-Fi and the LEDs, and the usermod runs the
// protocol from WLED's loop.
#define JL_WLED 0
#endif  // JL_WLED

#ifndef JL_LOG_HANDLER
// Whether logs go through a handler installed with SetLogHandler(), so that ESPHome or WLED can forward them to their
// own logging.
#if JL_ESPHOME || JL_WLED
#define JL_LOG_HANDLER 1
#else  // JL_ESPHOME || JL_WLED
#define JL_LOG_HANDLER 0
#endif  // JL_ESPHOME || JL_WLED
#endif  // JL_LOG_HANDLER

#ifndef JL_DISABLE_BLUETOOTH
#define JL_DISABLE_BLUETOOTH 0
#endif  // JL_DISABLE_BLUETOOTH

#ifndef JL_ESP32_NIMBLE
// Use the NimBLE Bluetooth stack instead of Bluedroid, which results in a smaller binary. With `framework = arduino`
// this requires the NimBLE-Arduino library, and with `framework = espidf, arduino` this requires
// sdkconfig.nimble.defaults.
#define JL_ESP32_NIMBLE 0
#endif  // JL_ESP32_NIMBLE

#ifndef JL_CORE2AWS_ETHERNET
#define JL_CORE2AWS_ETHERNET 0
#endif  // JL_CORE2AWS_ETHERNET

#ifndef JL_ETHERNET
#if JL_CORE2AWS_ETHERNET
#define JL_ETHERNET 1
#else  // JL_CORE2AWS_ETHERNET
#define JL_ETHERNET 0
#endif  // JL_CORE2AWS_ETHERNET
#endif  // JL_ETHERNET

#ifndef JL_ESP32_ETHERNET
// Esp32WiFiNetwork is currently work-in-progress.
// TODO: finish this and use it to replace ArduinoEspWiFiNetwork.
#define JL_ESP32_ETHERNET 0
#endif  // JL_ESP32_ETHERNET

#ifndef JL_SECURE_CHANNEL
// The secure channel uses lwIP sockets and mbedTLS, see extras/docs/OTA.md. Environments that only have
// Ethernet through the Arduino Ethernet library don't use lwIP, and ESPHome and WLED builds have their own OTA updates.
// WARNING: the session keys come from esp_fill_random(), which is only truly random while Wi-Fi or Bluetooth is
// running. So Ethernet-only builds with Bluetooth disabled, such as gauntlet_esp_ethernet_only, get weaker
// cryptography. That's acceptable only because those builds are just for testing.
#if defined(ESP32) && (JL_WIFI || JL_ESP32_ETHERNET) && !JL_ESPHOME && !JL_WLED
#define JL_SECURE_CHANNEL 1
#else  // ESP32 && lwIP && !ESPHOME && !WLED
#define JL_SECURE_CHANNEL 0
#endif  // ESP32 && lwIP && !ESPHOME && !WLED
#endif  // JL_SECURE_CHANNEL

#ifndef JL_OTA
#define JL_OTA JL_SECURE_CHANNEL
#endif  // JL_OTA

#if JL_OTA && !JL_SECURE_CHANNEL
#error "JL_OTA requires JL_SECURE_CHANNEL"
#endif  // JL_OTA && !JL_SECURE_CHANNEL

#ifndef JL_SECURE_CHANNEL_ECHO
// Enables a TLV that echoes its Value, used to test the channel without touching flash. Only for test builds.
#define JL_SECURE_CHANNEL_ECHO 0
#endif  // JL_SECURE_CHANNEL_ECHO

#ifndef JL_ORRERY_SUN
#define JL_ORRERY_SUN 0
#endif  // JL_ORRERY_SUN

#ifndef JL_ORRERY_PLUTO
#define JL_ORRERY_PLUTO 0
#endif  // JL_ORRERY_PLUTO

#ifndef JL_MAX485_BUS
#if defined(ESP32) && (JL_IS_CONFIG(ORRERY_LEADER) || JL_IS_CONFIG(ORRERY_PLANET)) && !JL_ORRERY_PLUTO
#define JL_MAX485_BUS 1
#else  // ESP32 && ORRERY
#define JL_MAX485_BUS 0
#endif  // ESP32 && ORRERY
#endif  // JL_MAX485_BUS

#ifndef JL_HALL_SENSOR
#define JL_HALL_SENSOR 0
#endif  // JL_HALL_SENSOR

#ifndef JL_MOTOR
#define JL_MOTOR 0
#endif  // JL_MOTOR

#ifndef JL_IS_M5_DEVICE
#if defined(__has_include) && __has_include(<M5Unified.h>)
#define JL_IS_M5_DEVICE 1
#else  // __has_include(<M5Unified.h>)
#define JL_IS_M5_DEVICE 0
#endif  // __has_include(<M5Unified.h>)
#endif  // JL_IS_M5_DEVICE

#ifndef JL_M5_LOGGING
// TODO: switch to JL_IS_M5_DEVICE when ready.
// #define JL_M5_LOGGING JL_IS_M5_DEVICE
#define JL_M5_LOGGING 0
#endif  // JL_M5_LOGGING

#ifndef JL_M5_EXT_POWER
#if JL_IS_CONTROLLER(M5STICK_C)
#define JL_M5_EXT_POWER 1
#else  // M5STICK_C
#define JL_M5_EXT_POWER 0
#endif  // M5STICK_C
#endif  // JL_M5_EXT_POWER

// For all of these GROVE ports, 1 is white and 2 is yellow.
#if JL_IS_CONTROLLER(ATOM_MATRIX) || JL_IS_CONTROLLER(ATOM_LITE)
inline constexpr int kPinA1 = 21;
inline constexpr int kPinA2 = 25;
inline constexpr int kPinB1 = 33;
inline constexpr int kPinB2 = 23;
inline constexpr int kPinC1 = 19;
inline constexpr int kPinC2 = 22;
#elif JL_IS_CONTROLLER(ATOM_S3)
inline constexpr int kPinA1 = 39;
inline constexpr int kPinA2 = 38;
inline constexpr int kPinB1 = 8;
inline constexpr int kPinB2 = 7;
inline constexpr int kPinC1 = 6;
inline constexpr int kPinC2 = 5;
#elif JL_IS_CONTROLLER(M5STICK_C)
inline constexpr int kPinA1 = 26;
inline constexpr int kPinA2 = 25;
inline constexpr int kPinB1 = 0;
#elif JL_IS_CONTROLLER(CORES3)
inline constexpr int kPinA1 = 1;
inline constexpr int kPinA2 = 2;
inline constexpr int kPinB1 = 8;
inline constexpr int kPinB2 = 9;
inline constexpr int kPinC1 = 18;
inline constexpr int kPinC2 = 17;
inline constexpr int kPinE1_1 = 6;
inline constexpr int kPinE1_3 = 0;
inline constexpr int kPinE2_2 = 5;
#elif JL_IS_CONTROLLER(CORE2AWS)
inline constexpr int kPinA1 = 33;
inline constexpr int kPinA2 = 32;
// B1 is unusable for GPIO on Core2AWS.
// inline constexpr int kPinB1 = 36;
inline constexpr int kPinB2 = 26;
inline constexpr int kPinC1 = 13;
inline constexpr int kPinC2 = 14;
inline constexpr int kPinE1_1 = 27;
inline constexpr int kPinE1_3 = 0;
inline constexpr int kPinE2_2 = 25;
#endif

#if JL_IS_CONTROLLER(CORES3) || JL_IS_CONTROLLER(CORE2AWS)
inline constexpr int kPinE1 = kPinE1_1;
inline constexpr int kPinE2 = kPinE2_2;
#endif  // Core

#endif  // JL_UTIL_CONFIG_H
