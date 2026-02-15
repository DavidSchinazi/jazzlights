#ifndef JL_CONFIG_H
#define JL_CONFIG_H

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
#define JL_CONFIG_ORRERY 14

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
#define JL_CONTROLLER_ATOM_S3_LITE 7
#define JL_CONTROLLER_M5STAMP_S3 8
#define JL_CONTROLLER_M5_NANO_C6 9

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
#define REVISION 14
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

#ifndef JL_MAX485_BUS
#if defined(ESP32) && JL_IS_CONFIG(ORRERY)
#define JL_MAX485_BUS 1
#else  // ESP32 && ORRERY
#define JL_MAX485_BUS 0
#endif  // ESP32 && ORRERY
#endif  // JL_MAX485_BUS

#ifndef JL_BUS_LEADER
#define JL_BUS_LEADER 0
#endif  // JL_BUS_LEADER

#endif  // JL_CONFIG_H
