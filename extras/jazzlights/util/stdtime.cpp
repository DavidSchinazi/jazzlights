#include "jazzlights/util/time.hpp"
#include <chrono>
#include <thread>
#include <stdlib.h>
namespace jazzlights {

Milliseconds timeMillis() {
  static auto t0 = std::chrono::steady_clock::now();
  const int64_t time64 =
    std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - t0).count();
  if (time64 > static_cast<int64_t>(std::numeric_limits<Milliseconds>::max() - 3600000)) {
    // Crash if we get within one hour of overflowing our Milliseconds type.
    // This happens after 24 days and 19 hours.
    abort();
  }
  return static_cast<Milliseconds>(time64);
}

}  // namespace jazzlights



