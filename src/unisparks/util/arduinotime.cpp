#include "unisparks/util/time.hpp"
#include <Arduino.h>
namespace unisparks {

Milliseconds timeMillis() {
  return ::millis();
}

void delay(Milliseconds duration) {
  ::delay(duration);
}

} // namespace unisparks



