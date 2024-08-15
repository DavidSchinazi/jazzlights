#ifndef JL_GPIO_BUTTON_H
#define JL_GPIO_BUTTON_H

#include <atomic>

#include "jazzlights/config.h"

#ifdef ESP32

#include "jazzlights/util/time.h"

namespace jazzlights {

// Allows tracking buttons connected to ESP32 GPIO pins.
class GpioButton {
 public:
  // Virtual interface class that will receive button callbacks.
  class Interface {
   public:
    virtual ~Interface() = default;
    // Called after the button is released, when it was pressed for less than 1s.
    virtual void ShortPress(uint8_t pin, Milliseconds currentTime) = 0;
    // Called after the button has been pressed for 1s (but while it is still pressed).
    virtual void LongPress(uint8_t pin, Milliseconds currentTime) = 0;
    // Called every second after LongPress while the button is still held down.
    virtual void HeldDown(uint8_t pin, Milliseconds currentTime) = 0;
  };

  // Starts tracking a button connected to a GPIO pin.
  explicit GpioButton(uint8_t pin, Interface& interface, Milliseconds currentTime);
  ~GpioButton();

  // Called once per primary runloop.
  void RunLoop(Milliseconds currentTime);

  // Returns whether the button is currently pressed.
  bool IsPressed(Milliseconds currentTime);

  // Returns whether the button is pressed and has been pressed for more than 1s.
  bool HasBeenPressedLongEnoughForLongPress(Milliseconds currentTime);

 private:
  static void InterruptHandler(void* arg);
  static void ConfigurePin(void* arg);
  void HandleInterrupt();
  Interface& interface_;
  std::atomic<Milliseconds> lastRawChange_;
  Milliseconds lastEvent_;
  std::atomic<bool> isPressedRaw_;
  bool isPressedDebounced_;
  bool isHeld_;
  const uint8_t pin_;
};

}  // namespace jazzlights

#endif  // ESP32
#endif  // JL_GPIO_BUTTON_H
