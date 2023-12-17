#include "jazzlights/gpio_button.h"

#ifdef ARDUINO

#include <Arduino.h>

#include "jazzlights/util/time.h"

namespace jazzlights {
namespace {
static constexpr Milliseconds kDebounceTime = 20;
static constexpr Milliseconds kLongPressTime = 1000;
}  // namespace

// Arduino's digitalRead() function can return spurious results sometimes. In particular when transitioning from
// not-pressed to pressed, it might rapidly alternate between the two a few times before settling on the correct value.
// To avoid reacting to these, we debounce the digital reads and only react after the value has settled for
// kDebounceTime. See <https://docs.arduino.cc/built-in-examples/digital/Debounce> for details.

GpioButton::GpioButton(uint8_t pin, Interface& interface, Milliseconds currentMillis)
    : interface_(interface),
      lastRawChange_(currentMillis),
      lastEvent_(currentMillis),
      isPressedRaw_(false),
      isPressedDebounced_(false),
      isHeld_(false),
      pin_(pin) {
  pinMode(pin_, INPUT_PULLUP);
}

void GpioButton::RunLoop(Milliseconds currentMillis) {
  const bool newIsPressed = (digitalRead(pin_) == LOW);
  if (newIsPressed != isPressedRaw_) {
    // Start debounce timer.
    isPressedRaw_ = newIsPressed;
    lastRawChange_ = currentMillis;
  }
  if (currentMillis - lastRawChange_ > kDebounceTime) {
    // GpioButton has been in this state longer than the debounce time.
    if (newIsPressed != isPressedDebounced_) {
      // GpioButton is transitioning states (with debouncing taken into account).
      isPressedDebounced_ = newIsPressed;
      if (isPressedDebounced_) {
        // GpioButton was just pressed, record time.
        lastEvent_ = currentMillis;
      } else {
        // GpioButton was just released.
        if (!isHeld_ && currentMillis - lastEvent_ < kLongPressTime) {
          // GpioButton was released after a short duration.
          lastEvent_ = currentMillis;
          interface_.ShortPress(pin_, currentMillis);
        }
        isHeld_ = false;
      }
    }
  }
  if (isPressedDebounced_ && currentMillis - lastEvent_ >= kLongPressTime) {
    // GpioButton has been held down for kLongPressTime since last event.
    lastEvent_ = currentMillis;
    if (!isHeld_) {
      // GpioButton has been held down for the first kLongPressTime.
      isHeld_ = true;
      interface_.LongPress(pin_, currentMillis);
    } else {
      // GpioButton has been held down for another kLongPressTime.
      interface_.HeldDown(pin_, currentMillis);
    }
  }
}

bool GpioButton::IsPressed(Milliseconds /*currentMillis*/) { return isPressedDebounced_; }
bool GpioButton::HasBeenPressedLongEnoughForLongPress(Milliseconds currentMillis) {
  return isPressedDebounced_ && (isHeld_ || currentMillis - lastEvent_ >= kLongPressTime);
}

}  // namespace jazzlights

#endif  // ARDUINO
