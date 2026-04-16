#include "jazzlights/ui/hall_sensor.h"

#if JL_HALL_SENSOR

#ifndef ESP32
#error "Hall sensor only supported on ESP32"
#endif

#include "jazzlights/ui/gpio_button.h"
#include "jazzlights/util/log.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

namespace {
constexpr int64_t kHallSensorDebounceDuration = 20000;  // 20ms.
}  // namespace

HallSensor::HallSensor(uint8_t pin, HallSensorInterface& interface)
    : pin_(pin, *this, kHallSensorDebounceDuration), interface_(interface) {}

void HallSensor::HandleChange(uint8_t pin, bool isClosed, int64_t timeOfChange) {
  interface_.HandleHallSensorChange(pin, isClosed, timeMillisFromEspTime(timeOfChange));
}

}  // namespace jazzlights

#endif  // JL_HALL_SENSOR
