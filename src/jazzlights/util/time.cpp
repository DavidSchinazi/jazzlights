#include "jazzlights/util/time.h"

#include <stdlib.h>

#include <limits>

#ifdef ESP32
#include "esp_timer.h"
#else  // ESP32
#include <chrono>
#endif  // ESP32

namespace jazzlights {

Milliseconds timeMillis() {
#ifdef ESP32
  const int64_t systemTime = esp_timer_get_time() / 1000;
#else   // ESP32
  static auto t0 = std::chrono::steady_clock::now();
  const int64_t systemTime =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
#endif  // ESP32

  if (systemTime > static_cast<int64_t>(std::numeric_limits<Milliseconds>::max() - 3600000)) {
    // Crash if we get within one hour of overflowing our Milliseconds type.
    // This happens after 24 days and 19 hours and will cause the program to restart.
    abort();
  }

  return static_cast<Milliseconds>(systemTime);
}

}  // namespace jazzlights
