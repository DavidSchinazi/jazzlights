#include "jazzlights/ui/ui_atom_s3.h"

#ifdef ARDUINO
#if JL_IS_CONTROLLER(ATOM_S3)

#include <M5Unified.h>

namespace jazzlights {
namespace {
static constexpr uint8_t kButtonPin = 41;
static constexpr Milliseconds kButtonLockTimeout = 10000;                     // 10s.
static constexpr Milliseconds kButtonLockTimeoutDuringUnlockSequence = 4000;  // 4s.

static constexpr uint8_t kBrightnessList[] = {2, 4, 8, 16, 32, 64, 128, 255};
static constexpr uint8_t kNumBrightnesses = sizeof(kBrightnessList) / sizeof(kBrightnessList[0]);

#if JL_DEV
static constexpr uint8_t kInitialBrightnessCursor = 0;
#elif JL_IS_CONFIG(STAFF)
static constexpr uint8_t kInitialBrightnessCursor = 3;
#elif JL_IS_CONFIG(HAMMER)
static constexpr uint8_t kInitialBrightnessCursor = 7;
#else
static constexpr uint8_t kInitialBrightnessCursor = 4;
#endif
}  // namespace

AtomS3Ui::AtomS3Ui(Player& player, Milliseconds currentTime)
    : ArduinoUi(player, currentTime),
      button_(kButtonPin, *this, currentTime),
      brightnessCursor_(kInitialBrightnessCursor) {}

bool AtomS3Ui::IsLocked() {
#if JL_BUTTON_LOCK
  return buttonLockState_ < 5;
#else   // JL_BUTTON_LOCK
  return false;
#endif  // JL_BUTTON_LOCK
}

void AtomS3Ui::HandleUnlockSequence(bool wasLongPress, Milliseconds currentTime) {
  if (!IsLocked()) { return; }
  // If we don’t receive the correct button event for the state we’re currently in, return immediately to state 0.
  // In odd states (1,3) we want a long press; in even states (0,2) we want a short press.
  if (((buttonLockState_ % 2) == 1) != wasLongPress) {
    buttonLockState_ = 0;
  } else {
    buttonLockState_++;
    // To reject accidental presses, exit unlock sequence if four seconds without progress
    lockButtonTime_ = currentTime + kButtonLockTimeoutDuringUnlockSequence;
  }
}

void AtomS3Ui::InitialSetup(Milliseconds currentTime) {
  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Display.init();
}

void AtomS3Ui::FinalSetup(Milliseconds currentTime) { UpdateScreen(currentTime); }

void AtomS3Ui::RunLoop(Milliseconds currentTime) {
  button_.RunLoop(currentTime);
  M5.update();

#if JL_BUTTON_LOCK

  // If idle-time expired, return to ‘locked’ state
  if (buttonLockState_ != 0 && currentTime - lockButtonTime_ >= 0) {
    jll_info("%u Locking buttons", currentTime);
    buttonLockState_ = 0;
  }

  if (IsLocked()) {
    // We show a blank display:
    // 1. When in fully locked mode, with the button not pressed
    // 2. When the button has been pressed long enough to register as a long press, and we want to signal the user to
    // let go now
    // 3. In the final transition from state 4 (awaiting release) to state 5 (unlocked)
    if ((buttonLockState_ == 0 && !button_.IsPressed(currentTime)) ||
        button_.HasBeenPressedLongEnoughForLongPress(currentTime) || buttonLockState_ >= 4) {
      M5.Display.sleep();
      M5.Display.clearDisplay();
      displayLongPress_ = false;
      displayShortPress_ = false;
    } else if ((buttonLockState_ % 2) == 1) {
      // In odd  states (1,3) we show "L".
      if (!displayLongPress_) {
        displayLongPress_ = true;
        M5.Display.wakeup();
        M5.Display.clearDisplay(::ORANGE);
        M5.Display.setTextColor(::BLACK, ::ORANGE);
        M5.Display.drawCenterString("Long Press", 64, 51, &fonts::Font4);
      }
      displayShortPress_ = false;
    } else {
      // In even states (0,2) we show "S".
      if (!displayShortPress_) {
        displayShortPress_ = true;
        M5.Display.wakeup();
        M5.Display.clearDisplay(::CYAN);
        M5.Display.setTextColor(::BLACK, ::CYAN);
        M5.Display.drawCenterString("Short Press", 64, 51, &fonts::Font4);
      }
      displayLongPress_ = false;
    }

    // In lock state 4, wait for release of the button, and then move to state 5 (fully unlocked)
    if (buttonLockState_ < 4 || button_.IsPressed(currentTime)) { return; }
    buttonLockState_ = 5;
    lockButtonTime_ = currentTime + kButtonLockTimeout;
    UpdateScreen(currentTime);
  } else if (button_.IsPressed(currentTime)) {
    lockButtonTime_ = currentTime + kButtonLockTimeout;
  }
#endif  // JL_BUTTON_LOCK
}

void AtomS3Ui::ShortPress(uint8_t pin, Milliseconds currentTime) {
  if (pin != kButtonPin) { return; }
  jll_info("%u ShortPress", currentTime);
  HandleUnlockSequence(/*wasLongPress=*/false, currentTime);
  if (IsLocked()) { return; }

  // Act on current menu mode.
  switch (menuMode_) {
    case MenuMode::kNext:
      jll_info("%u Next button has been hit", currentTime);
      player_.stopSpecial(currentTime);
      player_.stopLooping(currentTime);
      player_.next(currentTime);
      break;
    case MenuMode::kPrevious:
      jll_info("%u Back button has been hit", currentTime);
      player_.stopSpecial(currentTime);
      player_.loopOne(currentTime);
      break;
    case MenuMode::kBrightness:
      brightnessCursor_ = (brightnessCursor_ + 1 < kNumBrightnesses) ? brightnessCursor_ + 1 : 0;
      jll_info("%u Brightness button has been hit %u", currentTime, kBrightnessList[brightnessCursor_]);
      player_.SetBrightness(kBrightnessList[brightnessCursor_]);
      break;
    case MenuMode::kSpecial:
      jll_info("%u Special button has been hit", currentTime);
      player_.handleSpecial(currentTime);
      break;
  }
}

void AtomS3Ui::LongPress(uint8_t pin, Milliseconds currentTime) {
  if (pin != kButtonPin) { return; }
  jll_info("%u LongPress", currentTime);
  HandleUnlockSequence(/*wasLongPress=*/true, currentTime);
  if (IsLocked()) { return; }

  // Move to next menu mode.
  switch (menuMode_) {
    case MenuMode::kNext: menuMode_ = MenuMode::kPrevious; break;
    case MenuMode::kPrevious: menuMode_ = MenuMode::kBrightness; break;
    case MenuMode::kBrightness: menuMode_ = MenuMode::kNext; break;
    case MenuMode::kSpecial: menuMode_ = MenuMode::kNext; break;
  }
  UpdateScreen(currentTime);
}

void AtomS3Ui::HeldDown(uint8_t pin, Milliseconds currentTime) {
  if (pin != kButtonPin) { return; }
  jll_info("%u HeldDown", currentTime);
  if (IsLocked()) {
    // Button was held too long, go back to beginning of unlock sequence.
    buttonLockState_ = 0;
    return;
  }
  menuMode_ = MenuMode::kSpecial;
  UpdateScreen(currentTime);
}

void AtomS3Ui::UpdateScreen(Milliseconds currentTime) {
  M5.Display.wakeup();
  displayLongPress_ = false;
  displayShortPress_ = false;
  switch (menuMode_) {
    case MenuMode::kNext: {
      M5.Display.clearDisplay(::BLUE);
      M5.Display.setTextColor(::WHITE, ::BLUE);
      M5.Display.drawCenterString("Next", 64, 51, &fonts::Font4);
    } break;
    case MenuMode::kPrevious: {
      M5.Display.clearDisplay(::RED);
      M5.Display.setTextColor(::WHITE, ::RED);
      M5.Display.drawCenterString("Loop", 64, 51, &fonts::Font4);
    } break;
    case MenuMode::kBrightness: {
      M5.Display.clearDisplay(::YELLOW);
      M5.Display.setTextColor(::BLACK, ::YELLOW);
      M5.Display.drawCenterString("Bright", 64, 51, &fonts::Font4);
    } break;
    case MenuMode::kSpecial: {
      M5.Display.clearDisplay(::PURPLE);
      M5.Display.setTextColor(::WHITE, ::PURPLE);
      M5.Display.drawCenterString("Special", 64, 51, &fonts::Font4);
    } break;
  }
}

}  // namespace jazzlights

#endif  // JL_IS_CONTROLLER(ATOM_S3)
#endif  // ARDUINO
