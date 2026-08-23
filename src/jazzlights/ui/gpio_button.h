#ifndef JL_UI_GPIO_BUTTON_H
#define JL_UI_GPIO_BUTTON_H

#include "jazzlights/util/config.h"

#ifdef ESP32

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <optional>

#include "jazzlights/util/time.h"

namespace jazzlights {

// Allows tracking ESP32 GPIO pins.
class GpioPin {
 public:
  // Virtual interface class that will receive pin callbacks.
  class PinInterface {
   public:
    virtual ~PinInterface() = default;
    // Called when the pin state changes, after handling debouncing.
    virtual void HandleChange(uint8_t changedPin, bool isClosed, Microseconds timeOfChange) = 0;
  };

  // Starts tracking a GPIO pin.
  explicit GpioPin(uint8_t pin, PinInterface& pinInterface, Microseconds debounceDuration, bool closedIsHigh = false);
  ~GpioPin();

  // Called once per primary runloop.
  void RunLoop();

  // Returns GPIO pin number.
  uint8_t pin() const { return pin_; }

  // Returns whether the pin is currently closed.
  bool IsClosed() const { return isClosedDebouncedRunloop_; }

 private:
  static void InterruptHandler(void* arg);
  static void ConfigurePin(void* arg);
  void HandleInterrupt();

  PinInterface& pinInterface_;
  QueueHandle_t queue_;
  OptionalMicroseconds lastRunloopQueueEventTime_;
  OptionalMicroseconds lastChangeAwayFromDebounced_;
  bool lastIsClosedInISR_ = false;
  bool isClosedRawRunloop_ = false;
  bool isClosedDebouncedRunloop_ = false;
  const uint8_t pin_;
  const Microseconds debounceDuration_;
  const bool closedIsHigh_;
};

// Allows tracking buttons connected to ESP32 GPIO pins.
class GpioButton : public GpioPin::PinInterface {
 public:
  // Virtual interface class that will receive button callbacks.
  class ButtonInterface {
   public:
    virtual ~ButtonInterface() = default;
    // Called after the button is released, when it was pressed for less than 1s.
    virtual void ShortPress(uint8_t pin) = 0;
    // Called after the button has been pressed for 1s (but while it is still pressed).
    virtual void LongPress(uint8_t pin) = 0;
    // Called every second after LongPress while the button is still held down.
    virtual void HeldDown(uint8_t pin) = 0;
  };

  // Starts tracking a button connected to a GPIO pin.
  explicit GpioButton(uint8_t pin, ButtonInterface& buttonInterface);
  ~GpioButton();

  // Called once per primary runloop.
  void RunLoop();

  // Returns GPIO pin number.
  uint8_t pin() const { return gpioPin_.pin(); }

  // Returns whether the button is currently pressed.
  bool IsPressed() const { return gpioPin_.IsClosed(); }

  // Returns whether the button is pressed and has been pressed for more than 1s.
  bool HasBeenPressedLongEnoughForLongPress();

  // From GpioPin::PinInterface.
  void HandleChange(uint8_t pin, bool isClosed, Microseconds timeOfChange) override;

 private:
  GpioPin gpioPin_;
  ButtonInterface& buttonInterface_;
  OptionalMicroseconds lastEvent_;
  bool isHeld_ = false;
};

// Virtual interface class that will receive switch callbacks.
class GpioSwitchInterface {
 public:
  virtual ~GpioSwitchInterface() = default;
  // Called when the switch state changes.
  virtual void StateChanged(uint8_t pin, bool isClosed) = 0;
};

// Allows tracking switches connected to ESP32 GPIO pins.
template <bool closedIsHigh = true>
class GpioSwitch : public GpioPin::PinInterface {
 public:
  using SwitchInterface = GpioSwitchInterface;

  // Starts tracking a switch connected to a GPIO pin.
  explicit GpioSwitch(uint8_t pin, SwitchInterface& switchInterface);
  virtual ~GpioSwitch();

  // Called once per primary runloop.
  void RunLoop();

  // Returns GPIO pin number.
  uint8_t pin() const { return gpioPin_.pin(); }

  // Returns whether the switch is currently closed.
  bool IsClosed() const { return gpioPin_.IsClosed(); }

  // From GpioPin::PinInterface.
  void HandleChange(uint8_t pin, bool isClosed, Microseconds timeOfChange) override;

 private:
  GpioPin gpioPin_;
  SwitchInterface& switchInterface_;
};

using GpioSwitchHigh = GpioSwitch<true>;
using GpioSwitchLow = GpioSwitch<false>;

}  // namespace jazzlights

#endif  // ESP32
#endif  // JL_UI_GPIO_BUTTON_H
