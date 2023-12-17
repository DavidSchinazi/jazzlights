#ifndef JL_UI_ATOM_MATRIX_H
#define JL_UI_ATOM_MATRIX_H

#include "jazzlights/ui.h"

#ifdef ARDUINO
#if JL_IS_CONTROLLER(ATOM_MATRIX)

#include "jazzlights/gpio_button.h"

namespace jazzlights {

class AtomMatrixUi : public ArduinoUi, public GpioButton::Interface {
 public:
  explicit AtomMatrixUi(Player& player, Milliseconds currentTime);
  // From ArduinoUi.
  void InitialSetup(Milliseconds currentTime) override;
  void FinalSetup(Milliseconds currentTime) override;
  void RunLoop(Milliseconds currentTime) override;
  // From GpioButton::Interface.
  void ShortPress(uint8_t pin, Milliseconds currentTime) override;
  void LongPress(uint8_t pin, Milliseconds currentTime) override;
  void HeldDown(uint8_t pin, Milliseconds currentTime) override;

 private:
  bool IsLocked();
  void HandleUnlockSequence(bool wasLongPress, Milliseconds currentTime);
  bool atomScreenMessage(const Milliseconds currentTime);

  GpioButton button_;
  bool displayingBootMessage_ = true;
  // Definitions of the button lock states:
  // 0 Awaiting short press
  // 1 Awaiting long press
  // 2 Awaiting short press
  // 3 Awaiting long press
  // 4 Awaiting release
  // 5 Unlocked
  uint8_t buttonLockState_ = 0;
  Milliseconds lockButtonTime_ = 0;  // Time at which we'll lock the buttons.
};

}  // namespace jazzlights

#endif  // JL_IS_CONTROLLER(ATOM_MATRIX)
#endif  // ARDUINO
#endif  // JL_UI_ATOM_MATRIX_H