#include "jazzlights/util/time.h"

#include <stdlib.h>

#include <limits>

#ifdef ESP32
#include "esp_timer.h"
#else  // ESP32
#include <chrono>
#endif  // ESP32

namespace jazzlights {

static constexpr Microseconds kMillisecondsPerMicroseconds = 1000;

static constexpr Microseconds kTimeStartOffset = 100000000;  // 100s.

Milliseconds timeMillis() { return MicrosecondsToMilliseconds(timeMicros()); }

Microseconds timeMicros() {
#ifdef ESP32
  // We measured this on the Atom Matrix ESP32 and it appears that timeMillis() takes about one microsecond and
  // esp_timer_get_time() takes about half a microsecond.
  const Microseconds systemTime = esp_timer_get_time();
#else   // ESP32
  static auto t0 = std::chrono::steady_clock::now();
  const Microseconds systemTime =
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count();
#endif  // ESP32

  // We add 100000 here to have the time start at 100s. That allows subtracting without having the time become negative.
  // In particular, without this addition, we would not properly handle received pattern time sync messages because the
  // currentPatternStartTime could become negative and would be clamped at zero.
  return systemTime + kTimeStartOffset;
}

Milliseconds MicrosecondsToMilliseconds(Microseconds timeMicros) {
  if (timeMicros > static_cast<Microseconds>(std::numeric_limits<Milliseconds>::max() - 3600000)) {
    // Crash if we get within one hour of overflowing our Milliseconds type.
    // This happens after 24 days and 19 hours and will cause the program to restart.
    abort();
  }
  return timeMicros / kMillisecondsPerMicroseconds;
}

}  // namespace jazzlights
