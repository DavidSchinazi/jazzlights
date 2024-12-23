#ifndef JL_ESP32_SHARED_H
#define JL_ESP32_SHARED_H

#include "jazzlights/config.h"

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

}  // namespace jazzlights

#endif  // ESP32
#endif  // JL_ESP32_SHARED_H
