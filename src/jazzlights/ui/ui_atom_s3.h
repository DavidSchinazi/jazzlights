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
  struct DisplayContents {
   public:
    enum class Mode {
      kUninitialized,
      kOff,
      kLockedShort,
      kLockedLong,
      kNext,
      kLoop,
      kBrightness,
      kSpecial,
    };

    DisplayContents() : mode(Mode::kUninitialized) {}
    explicit DisplayContents(Mode m) : mode(m) {}
    DisplayContents(const DisplayContents& other) { *this = other; }
    DisplayContents& operator=(const DisplayContents& other);
    bool operator==(const DisplayContents& other) const;
    bool operator!=(const DisplayContents& other) const { return !(*this == other); }

    Mode mode;
    union {
      struct {
        uint8_t brightness;
      } brightness;
    } c;
  };

  void Display(const DisplayContents& contents, Milliseconds currentTime);
  void UpdateScreen(Milliseconds currentTime);
  bool IsLocked();
  void HandleUnlockSequence(bool wasLongPress, Milliseconds currentTime);

  enum class MenuMode {
    kNext,
    kLoop,
    kBrightness,
    kSpecial,
  };

  DisplayContents lastDisplayedContents_;
  MenuMode menuMode_ = MenuMode::kNext;
  uint8_t brightnessCursor_;
  GpioButton button_;
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

#endif  // JL_IS_CONTROLLER(ATOM_S3)
#endif  // ARDUINO
#endif  // JL_UI_ATOM_S3_H
