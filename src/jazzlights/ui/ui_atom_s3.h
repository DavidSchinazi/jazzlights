#ifndef JL_UI_ATOM_S3_H
#define JL_UI_ATOM_S3_H

#include "jazzlights/ui/ui.h"

#ifdef ARDUINO
#if JL_IS_CONTROLLER(ATOM_S3)

#include "jazzlights/ui/gpio_button.h"

namespace jazzlights {

class AtomS3Ui : public ArduinoUi, public GpioButton::Interface {
 public:
  explicit AtomS3Ui(Player& player, Milliseconds currentTime);
  // From ArduinoUi.
  void InitialSetup(Milliseconds currentTime) override;
  void FinalSetup(Milliseconds currentTime) override;
  void RunLoop(Milliseconds currentTime) override;
  // From GpioButton::Interface.
  void ShortPress(uint8_t pin, Milliseconds currentTime) override;
  void LongPress(uint8_t pin, Milliseconds currentTime) override;
  void HeldDown(uint8_t pin, Milliseconds currentTime) override;

 private:
  GpioButton button_;
  bool foo_ = false;
};

}  // namespace jazzlights

#endif  // JL_IS_CONTROLLER(ATOM_S3)
#endif  // ARDUINO
#endif  // JL_UI_ATOM_S3_H
