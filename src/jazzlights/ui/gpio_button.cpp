#include "jazzlights/ui/gpio_button.h"

#ifdef ESP32

#include <driver/gpio.h>

#include "jazzlights/util/time.h"

namespace jazzlights {
namespace {
static constexpr Milliseconds kDebounceTime = 20;
static constexpr Milliseconds kLongPressTime = 1000;
}  // namespace

// GPIO reads can return spurious results sometimes. In particular when
// transitioning from not-pressed to pressed, it might rapidly alternate
// between the two a few times before settling on the correct value. To avoid
// reacting to these, we debounce the digital reads and only react after the
// value has settled for kDebounceTime. See
// <https://docs.arduino.cc/built-in-examples/digital/Debounce> for details.

GpioButton::GpioButton(uint8_t pin, Interface& interface, Milliseconds currentTime)
    : interface_(interface),
      lastRawChange_(currentTime),
      lastEvent_(currentTime),
      isPressedRaw_(false),
      isPressedDebounced_(false),
      isHeld_(false),
      pin_(pin) {
  static const gpio_config_t config = {
      .pin_bit_mask = (1ULL << pin_),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&config));
}

void GpioButton::RunLoop(Milliseconds currentTime) {
  const bool newIsPressed = gpio_get_level(static_cast<gpio_num_t>(pin_)) == 0;
  if (newIsPressed != isPressedRaw_) {
    // Start debounce timer.
    isPressedRaw_ = newIsPressed;
    lastRawChange_ = currentTime;
  }
  if (currentTime - lastRawChange_ > kDebounceTime) {
    // GpioButton has been in this state longer than the debounce time.
    if (newIsPressed != isPressedDebounced_) {
      // GpioButton is transitioning states (with debouncing taken into account).
      isPressedDebounced_ = newIsPressed;
      if (isPressedDebounced_) {
        // GpioButton was just pressed, record time.
        lastEvent_ = currentTime;
      } else {
        // GpioButton was just released.
        if (!isHeld_ && currentTime - lastEvent_ < kLongPressTime) {
          // GpioButton was released after a short duration.
          lastEvent_ = currentTime;
          interface_.ShortPress(pin_, currentTime);
        }
        isHeld_ = false;
      }
    }
  }
  if (isPressedDebounced_ && currentTime - lastEvent_ >= kLongPressTime) {
    // GpioButton has been held down for kLongPressTime since last event.
    lastEvent_ = currentTime;
    if (!isHeld_) {
      // GpioButton has been held down for the first kLongPressTime.
      isHeld_ = true;
      interface_.LongPress(pin_, currentTime);
    } else {
      // GpioButton has been held down for another kLongPressTime.
      interface_.HeldDown(pin_, currentTime);
    }
  }
}

bool GpioButton::IsPressed(Milliseconds /*currentTime*/) { return isPressedDebounced_; }
bool GpioButton::HasBeenPressedLongEnoughForLongPress(Milliseconds currentTime) {
  return isPressedDebounced_ && (isHeld_ || currentTime - lastEvent_ >= kLongPressTime);
}

}  // namespace jazzlights

#endif  // ESP32
