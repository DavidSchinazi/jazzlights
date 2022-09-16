#include "jazzlights/util/time.h"

#include <stdlib.h>
#include <limits>

#ifdef ARDUINO
#  include <Arduino.h>
#else  // ARDUINO
#  include <chrono>
#endif  // ARDUINO

namespace jazzlights {

Milliseconds timeMillis() {
#ifdef ARDUINO
  const unsigned long systemTime = ::millis();
#else  // ARDUINO
  static auto t0 = std::chrono::steady_clock::now();
  const int64_t systemTime =
    std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - t0).count();
#endif  // ARDUINO

  if (systemTime > static_cast<std::remove_const<decltype(systemTime)>::type>(std::numeric_limits<Milliseconds>::max() - 3600000)) {
    // Crash if we get within one hour of overflowing our Milliseconds type.
    // This happens after 24 days and 19 hours and will cause the program to restart.
    abort();
  }

  return static_cast<Milliseconds>(systemTime);
}

}  // namespace jazzlights

