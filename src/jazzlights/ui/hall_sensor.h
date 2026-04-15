#ifndef JL_UI_HALL_SENSOR_H
#define JL_UI_HALL_SENSOR_H

#include "jazzlights/config.h"

#if JL_HALL_SENSOR

#include "jazzlights/ui/gpio_button.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

class HallSensor : public GpioPin::PinInterface {
 public:
  explicit HallSensor(uint8_t pin);
  ~HallSensor() = default;
  bool IsClosed() const { return pin_.IsClosed(); }
  Milliseconds GetTimeLastOpened() const;

  // Called once per primary runloop.
  void RunLoop() { pin_.RunLoop(); }

  // From GpioPin::PinInterface.
  void HandleChange(uint8_t pin, bool isClosed, int64_t timeOfChange) override;

 private:
  GpioPin pin_;
  Milliseconds timeLastOpened_ = -1;
};

}  // namespace jazzlights

#endif  // JL_HALL_SENSOR
#endif  // JL_UI_HALL_SENSOR_H
