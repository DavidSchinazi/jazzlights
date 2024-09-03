#ifndef JL_ROTARY_PHONE_H
#define JL_ROTARY_PHONE_H

#include "jazzlights/config.h"

#ifdef ESP32
#if JL_IS_CONFIG(PHONE)

#include <cstdint>

#include "jazzlights/ui/gpio_button.h"

namespace jazzlights {

class PhonePinHandler : public GpioPin::PinInterface {
 public:
  static PhonePinHandler* Get();

  // Called once per primary runloop.
  void RunLoop();
  // From GpioPin::PinInterface.
  void HandleChange(uint8_t pin, bool isClosed, int64_t timeOfChange) override;

 private:
  PhonePinHandler();

  bool dialing_ = false;
  bool lastKnownDigitIsClosed_ = false;
  uint8_t digitCount_ = 0;
  int64_t lastDigitEvent_ = -1;
  uint64_t fullNumber_ = 0;
  int64_t lastNumberEvent_ = -1;
  GpioPin digitPin_;
  GpioPin dialPin_;
};

}  // namespace jazzlights

#endif  // PHONE
#endif  // ESP32
#endif  // JL_ROTARY_PHONE_H
