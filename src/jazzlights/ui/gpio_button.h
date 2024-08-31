#ifndef JL_GPIO_BUTTON_H
#define JL_GPIO_BUTTON_H

#include "jazzlights/config.h"

#ifdef ESP32

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace jazzlights {

// Allows tracking ESP32 GPIO pins.
class GpioPin {
 public:
  // Virtual interface class that will receive pin callbacks.
  class PinInterface {
   public:
    virtual ~PinInterface() = default;
    // Called when the pin state changes, after handling debouncing.
    virtual void HandleChange(uint8_t changedPin, bool isClosed, int64_t timeOfChange) = 0;
  };

  // Starts tracking a GPIO pin.
  explicit GpioPin(uint8_t pin, PinInterface& pinInterface);
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
  int64_t lastChangeAwayFromDebounced_;
  bool lastIsClosedInISR_;
  bool isClosedRawRunloop_;
  bool isClosedDebouncedRunloop_;
  const uint8_t pin_;
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
  void HandleChange(uint8_t pin, bool isClosed, int64_t timeOfChange) override;

 private:
  GpioPin gpioPin_;
  ButtonInterface& buttonInterface_;
  int64_t lastEvent_;
  bool isHeld_;
};

}  // namespace jazzlights

#endif  // ESP32
#endif  // JL_GPIO_BUTTON_H
