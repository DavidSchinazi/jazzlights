#include "dfsparks/timer.h"
#ifdef ARDUINO
#include <Arduino.h>
#else
#include <chrono>
#endif

namespace dfsparks {

int32_t timeMillis() {
#ifndef ARDUINO
  static auto t0 = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - t0)
      .count();
#else
  return millis();
#endif
}

} // namespace dfsparks
