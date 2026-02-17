#include "jazzlights/ui/hall_sensor.h"

#if JL_HALL_SENSOR

#ifndef ESP32
#error "Hall sensor only supported on ESP32"
#endif

#include "jazzlights/ui/gpio_button.h"
#include "jazzlights/util/log.h"

namespace jazzlights {

namespace {
constexpr int64_t kHallSensorDebounceDuration = 20000;  // 20ms.
}  // namespace

HallSensor::HallSensor(uint8_t pin) : pin_(pin, *this, kHallSensorDebounceDuration) {}

void HallSensor::HandleChange(uint8_t pin, bool isClosed, int64_t timeOfChange) {
  jll_info("Hall sensor at pin %d is now %s", static_cast<int>(pin), (isClosed ? "closed" : "open"));
}

}  // namespace jazzlights

#endif  // JL_HALL_SENSOR
