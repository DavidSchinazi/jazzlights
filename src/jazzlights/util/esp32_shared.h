#ifndef JL_UTIL_ESP32_SHARED_H
#define JL_UTIL_ESP32_SHARED_H

#include "jazzlights/util/config.h"

#ifdef ESP32

#include <esp_idf_version.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace jazzlights {

void InitializeNetStack();

void InstallGpioIsrService();

enum : UBaseType_t {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  kHighestTaskPriority = 24,
#else
  kHighestTaskPriority = 30,
#endif
};

#if JL_ARDUINO
// The Arduino core runs setup() and loop() from its loopTask, using these settings.
inline constexpr BaseType_t kPrimaryRunLoopCore = CONFIG_ARDUINO_RUNNING_CORE;
inline constexpr uint32_t kPrimaryRunLoopStackSize = CONFIG_ARDUINO_LOOP_STACK_SIZE;
#else   // JL_ARDUINO
// Without the Arduino core, main.cpp creates the primary runloop task itself. These match the Arduino defaults.
inline constexpr BaseType_t kPrimaryRunLoopCore = portNUM_PROCESSORS > 1 ? 1 : 0;
inline constexpr uint32_t kPrimaryRunLoopStackSize = 8192;
#endif  // JL_ARDUINO

}  // namespace jazzlights

#endif  // ESP32
#endif  // JL_UTIL_ESP32_SHARED_H
