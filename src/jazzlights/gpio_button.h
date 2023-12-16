#ifndef JL_GPIO_BUTTON_H
#define JL_GPIO_BUTTON_H

#include "jazzlights/config.h"

#ifdef ARDUINO

#include "jazzlights/util/time.h"

namespace jazzlights {

// Allows tracking buttons connected to Arduino GPIO pins.
class Button {
 public:
  // Virtual interface class that will receive button callbacks.
  class Interface {
   public:
    virtual ~Interface() = default;
    // Called after the button is released, when it was pressed for less than 1s.
    virtual void ShortPress(uint8_t pin, Milliseconds currentMillis) = 0;
    // Called after the button has been pressed for 1s (but while it is still pressed).
    virtual void LongPress(uint8_t pin, Milliseconds currentMillis) = 0;
    // Called every second after LongPress while the button is still held down.
    virtual void HeldDown(uint8_t pin, Milliseconds currentMillis) = 0;
  };

  // Starts tracking a button connected to a GPIO pin.
  explicit Button(uint8_t pin, Interface& interface, Milliseconds currentMillis);

  // Needs to be called on every iteration of the Arduino run loop.
  void RunLoop(Milliseconds currentMillis);

  // Returns whether the button is currently pressed.
  bool IsPressed(Milliseconds currentMillis);

  // Returns whether the button is pressed and has been pressed for more than 1s.
  bool HasBeenPressedLongEnoughForLongPress(Milliseconds currentMillis);

 private:
  Interface& interface_;
  Milliseconds lastRawChange_;
  Milliseconds lastEvent_;
  bool isPressedRaw_;
  bool isPressedDebounced_;
  bool isHeld_;
  const uint8_t pin_;
};

}  // namespace jazzlights

#endif  // ARDUINO
#endif  // JL_GPIO_BUTTON_H
