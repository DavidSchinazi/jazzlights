#ifndef JL_ESP32_SHARED_H
#define JL_ESP32_SHARED_H

#include "jazzlights/config.h"

#ifdef ESP32

namespace jazzlights {

void InitializeNetStack();

void InstallGpioIsrService();

}  // namespace jazzlights

#endif  // ESP32
#endif  // JL_ESP32_SHARED_H
