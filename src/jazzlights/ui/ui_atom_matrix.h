#ifndef JL_UI_UI_ATOM_MATRIX_H
#define JL_UI_UI_ATOM_MATRIX_H

#include "jazzlights/ui/ui.h"

#ifdef ESP32
#if JL_IS_CONTROLLER(ATOM_MATRIX)

#define ATOM_SCREEN_NUM_LEDS 25

#include "jazzlights/fastled_wrapper.h"
#include "jazzlights/ui/gpio_button.h"

namespace jazzlights {

class AtomMatrixUi : public Esp32Ui, public GpioButton::ButtonInterface {
 public:
  explicit AtomMatrixUi(Player& player);
  // From Esp32Ui.
  void InitialSetup() override;
  void FinalSetup() override;
  void RunLoop(Milliseconds currentTime) override;
  void set_fastled_runner(FastLedRunner* runner) override { runner_ = runner; }
  // From GpioButton::ButtonInterface.
  void ShortPress(uint8_t pin) override;
  void LongPress(uint8_t pin) override;
  void HeldDown(uint8_t pin) override;

 private:
  bool IsLocked();
  void HandleUnlockSequence(bool wasLongPress, Milliseconds currentTime);
  bool ScreenMessage(Milliseconds currentTime);
  void ScreenUnlocked(Milliseconds currentTime);
  void ScreenNetwork(Milliseconds currentTime);
  void ScreenDisplay(Milliseconds currentTime);
  void ScreenShort(Milliseconds currentTime);
  void ScreenLong(Milliseconds currentTime);
  void ScreenClear(Milliseconds currentTime);

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

  enum class MenuMode {
    kNext,
    kPrevious,
    kBrightness,
    kSpecial,
  };
  MenuMode menuMode_ = MenuMode::kNext;
  uint8_t brightnessCursor_;
  uint8_t brightnessLastWrite_ = 255;

  CRGB screenLEDs_[ATOM_SCREEN_NUM_LEDS] = {};
  CRGB screenLEDsLastWrite_[ATOM_SCREEN_NUM_LEDS] = {};
  FastLedRunner* runner_ = nullptr;  // Unowned.
};

}  // namespace jazzlights

#endif  // JL_IS_CONTROLLER(ATOM_MATRIX)
#endif  // ESP32
#endif  // JL_UI_UI_ATOM_MATRIX_H