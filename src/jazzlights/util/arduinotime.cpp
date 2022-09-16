#include "jazzlights/util/time.h"
#include <Arduino.h>
#include <limits>
#include <stdlib.h>

namespace jazzlights {

Milliseconds timeMillis() {
  const unsigned long unsignedTime = ::millis();
  if (unsignedTime > static_cast<unsigned long>(std::numeric_limits<Milliseconds>::max() - 3600000)) {
    // Reboot if we get within one hour of overflowing our Milliseconds type.
    // This happens after 24 days and 19 hours.
    abort();
  }
  return static_cast<Milliseconds>(unsignedTime);
}

}  // namespace jazzlights


