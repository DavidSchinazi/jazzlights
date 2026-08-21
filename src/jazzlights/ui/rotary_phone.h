#ifndef JL_UI_ROTARY_PHONE_H
#define JL_UI_ROTARY_PHONE_H

#include "jazzlights/config.h"

#ifdef ESP32
#if JL_IS_CONFIG(PHONE)

#include <cstdint>
#include <optional>

#include "jazzlights/ui/gpio_button.h"

namespace jazzlights {

class PhonePinHandler : public GpioPin::PinInterface {
 public:
  static PhonePinHandler* Get();

  // Called once per primary runloop.
  void RunLoop();
  // From GpioPin::PinInterface.
  void HandleChange(uint8_t pin, bool isClosed, Microseconds timeOfChange) override;

 private:
  PhonePinHandler();

  bool dialing_ = false;
  bool lastKnownDigitIsClosed_ = false;
  uint8_t digitCount_ = 0;
  std::optional<Microseconds> lastDigitEvent_;
  uint64_t fullNumber_ = 0;
  std::optional<Microseconds> lastNumberEvent_;
  GpioPin digitPin_;
  GpioPin dialPin_;
};

}  // namespace jazzlights

#endif  // PHONE
#endif  // ESP32
#endif  // JL_UI_ROTARY_PHONE_H
