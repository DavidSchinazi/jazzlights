#include "jazzlights/ui/gpio_button.h"

#ifdef ESP32

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "jazzlights/util/log.h"
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
  // GPIO interrupt handlers are run on the core where the config calls were made, so we create a short-lived task to
  // ensure all that happens on core 0. This prevents it from interfering with LED writes on core 1.
  TaskHandle_t taskHandle = nullptr;
  if (xTaskCreatePinnedToCore(ConfigurePin, "GPIO_int", configMINIMAL_STACK_SIZE,
                              /*parameters=*/this,
                              /*priority=*/30, &taskHandle, /*coreID=*/0) != pdPASS) {
    jll_fatal("Failed to create GPIO interrupt handler config task");
  }
}

// static
void GpioButton::ConfigurePin(void* arg) {
  GpioButton* button = reinterpret_cast<GpioButton*>(arg);
  static const gpio_config_t config = {
      .pin_bit_mask = (1ULL << button->pin_),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_ANYEDGE,
  };
  ESP_ERROR_CHECK(gpio_config(&config));
  // Ensure this is only ever called once by saving the result in a static variable.
  static esp_err_t err = gpio_install_isr_service(/*intr_alloc_flags=*/0);
  ESP_ERROR_CHECK(err);
  ESP_ERROR_CHECK(gpio_isr_handler_add(static_cast<gpio_num_t>(button->pin_), &GpioButton::InterruptHandler, button));
  // Delete this task.
  vTaskDelete(nullptr);
}

GpioButton::~GpioButton() { ESP_ERROR_CHECK(gpio_isr_handler_remove(static_cast<gpio_num_t>(pin_))); }

void GpioButton::RunLoop(Milliseconds currentTime) {
  const Milliseconds lastRawChange = lastRawChange_.load(std::memory_order_relaxed);
  if (currentTime - lastRawChange > kDebounceTime) {
    const bool isPressedRaw = isPressedRaw_.load(std::memory_order_relaxed);
    // GpioButton has been in this state longer than the debounce time.
    if (isPressedRaw != isPressedDebounced_) {
      // GpioButton is transitioning states (with debouncing taken into account).
      isPressedDebounced_ = isPressedRaw;
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

// static
void GpioButton::InterruptHandler(void* arg) { reinterpret_cast<GpioButton*>(arg)->HandleInterrupt(); }

void GpioButton::HandleInterrupt() {
  const bool newIsPressed = gpio_get_level(static_cast<gpio_num_t>(pin_)) == 0;
  const bool oldIsPressedRaw = isPressedRaw_.exchange(newIsPressed, std::memory_order_relaxed);
  if (newIsPressed != oldIsPressedRaw) { lastRawChange_.store(timeMillis(), std::memory_order_relaxed); }
}

}  // namespace jazzlights

#endif  // ESP32
