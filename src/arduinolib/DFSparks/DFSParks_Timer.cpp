#include "DFSparks_Timer.h"
#ifndef ARDUINO
#include <chrono>
#else
#include <ESP8266WiFi.h> // where is millis defined?!!
#endif

namespace dfsparks {

int32_t time_ms() {
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
