#include "jazzlights/ui/ui_atom_s3.h"

#ifdef ESP32
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

static void DisplayCenteredText(const char* text, int textColor, int backgroundColor) {
  M5.Display.clearDisplay(backgroundColor);
  M5.Display.setTextColor(textColor, backgroundColor);
  M5.Display.drawCenterString(text, /*x=*/64, /*y=*/51, &fonts::Font4);
}
static void DisplayCenteredText(const std::string& text, int textColor, int backgroundColor) {
  DisplayCenteredText(text.c_str(), textColor, backgroundColor);
}

}  // namespace

void AtomS3Ui::Display(const DisplayContents& contents, Milliseconds currentTime) {
  if (lastDisplayedContents_ == contents) {
    // Do not display if nothing has changed.
    return;
  }
  if ((lastDisplayedContents_.mode == DisplayContents::Mode::kUninitialized ||
       lastDisplayedContents_.mode == DisplayContents::Mode::kOff) &&
      (contents.mode != DisplayContents::Mode::kUninitialized) && (contents.mode != DisplayContents::Mode::kOff)) {
    // Turn screen on if it was previously off.
    M5.Display.wakeup();
  }
  switch (contents.mode) {
    case DisplayContents::Mode::kUninitialized: break;
    case DisplayContents::Mode::kOff: {
      M5.Display.clearDisplay();
      M5.Display.sleep();
    } break;
    case DisplayContents::Mode::kLockedShort: {
      DisplayCenteredText("Short Press", ::BLACK, ::CYAN);
    } break;
    case DisplayContents::Mode::kLockedLong: {
      DisplayCenteredText("Long Press", ::BLACK, ::ORANGE);
    } break;
    case DisplayContents::Mode::kNext: {
      DisplayCenteredText("Next", ::WHITE, ::BLUE);
      M5.Display.drawCenterString(patternName(contents.c.next.currentEffect).c_str(), /*x=*/64, /*y=*/80,
                                  &fonts::Font2);
    } break;
    case DisplayContents::Mode::kLoop: {
      DisplayCenteredText("Loop", ::WHITE, ::RED);
    } break;
    case DisplayContents::Mode::kBrightness: {
      DisplayCenteredText("Bright", ::BLACK, ::YELLOW);
      M5.Display.drawCenterString(std::to_string(contents.c.brightness.brightness).c_str(), /*x=*/64, /*y=*/80,
                                  &fonts::Font4);
    } break;
    case DisplayContents::Mode::kSpecial: {
      DisplayCenteredText("Special", ::WHITE, ::PURPLE);
    } break;
  }
  lastDisplayedContents_ = contents;
}

void AtomS3Ui::UpdateScreen(Milliseconds currentTime) {
  DisplayContents contents;
  switch (menuMode_) {
    case MenuMode::kNext: {
      contents = DisplayContents(DisplayContents::Mode::kNext);
      contents.c.next.currentEffect = player_.currentEffect();
    } break;
    case MenuMode::kLoop: {
      contents = DisplayContents(DisplayContents::Mode::kLoop);
    } break;
    case MenuMode::kBrightness: {
      contents = DisplayContents(DisplayContents::Mode::kBrightness);
      contents.c.brightness.brightness = player_.brightness();
    } break;
    case MenuMode::kSpecial: {
      contents = DisplayContents(DisplayContents::Mode::kSpecial);
    } break;
  }
  Display(contents, currentTime);
}

AtomS3Ui::AtomS3Ui(Player& player)
    : Esp32Ui(player), button_(kButtonPin, *this), brightnessCursor_(kInitialBrightnessCursor) {}

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

void AtomS3Ui::InitialSetup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.init();
  Display(DisplayContents(DisplayContents::Mode::kOff), timeMillis());
}

void AtomS3Ui::FinalSetup() {}

void AtomS3Ui::RunLoop(Milliseconds currentTime) {
  button_.RunLoop();
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
    if ((buttonLockState_ == 0 && !button_.IsPressed()) || button_.HasBeenPressedLongEnoughForLongPress() ||
        buttonLockState_ >= 4) {
      Display(DisplayContents(DisplayContents::Mode::kOff), currentTime);
    } else if ((buttonLockState_ % 2) == 1) {
      // In odd  states (1,3) we show "L".
      Display(DisplayContents(DisplayContents::Mode::kLockedLong), currentTime);
    } else {
      // In even states (0,2) we show "S".
      Display(DisplayContents(DisplayContents::Mode::kLockedShort), currentTime);
    }

    // In lock state 4, wait for release of the button, and then move to state 5 (fully unlocked)
    if (buttonLockState_ < 4 || button_.IsPressed()) { return; }
    buttonLockState_ = 5;
    lockButtonTime_ = currentTime + kButtonLockTimeout;
  } else if (button_.IsPressed()) {
    lockButtonTime_ = currentTime + kButtonLockTimeout;
  }
#endif  // JL_BUTTON_LOCK
  UpdateScreen(currentTime);
}

void AtomS3Ui::ShortPress(uint8_t pin) {
  if (pin != kButtonPin) { return; }
  const Milliseconds currentTime = timeMillis();
  jll_info("%u AtomS3Ui ShortPress", currentTime);
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
    case MenuMode::kLoop:
      jll_info("%u Loop button has been hit", currentTime);
      player_.stopSpecial(currentTime);
      player_.loopOne(currentTime);
      break;
    case MenuMode::kBrightness:
      brightnessCursor_ = (brightnessCursor_ + 1 < kNumBrightnesses) ? brightnessCursor_ + 1 : 0;
      jll_info("%u Brightness button has been hit %u", currentTime, kBrightnessList[brightnessCursor_]);
      player_.set_brightness(kBrightnessList[brightnessCursor_]);
      break;
    case MenuMode::kSpecial:
      jll_info("%u Special button has been hit", currentTime);
      player_.handleSpecial(currentTime);
      break;
  }
  UpdateScreen(currentTime);
}

void AtomS3Ui::LongPress(uint8_t pin) {
  if (pin != kButtonPin) { return; }
  const Milliseconds currentTime = timeMillis();
  jll_info("%u AtomS3Ui LongPress", currentTime);
  HandleUnlockSequence(/*wasLongPress=*/true, currentTime);
  if (IsLocked()) { return; }

  // Move to next menu mode.
  switch (menuMode_) {
    case MenuMode::kNext: menuMode_ = MenuMode::kLoop; break;
    case MenuMode::kLoop: menuMode_ = MenuMode::kBrightness; break;
    case MenuMode::kBrightness: menuMode_ = MenuMode::kNext; break;
    case MenuMode::kSpecial: menuMode_ = MenuMode::kNext; break;
  }
  UpdateScreen(currentTime);
}

void AtomS3Ui::HeldDown(uint8_t pin) {
  if (pin != kButtonPin) { return; }
  const Milliseconds currentTime = timeMillis();
  jll_info("%u AtomS3Ui HeldDown", currentTime);
  if (IsLocked()) {
    // Button was held too long, go back to beginning of unlock sequence.
    buttonLockState_ = 0;
    return;
  }
  menuMode_ = MenuMode::kSpecial;
  UpdateScreen(currentTime);
}

AtomS3Ui::DisplayContents& AtomS3Ui::DisplayContents::operator=(const DisplayContents& other) {
  mode = other.mode;
  switch (mode) {
    case Mode::kUninitialized: break;
    case Mode::kOff: break;
    case Mode::kLockedShort: break;
    case Mode::kLockedLong: break;
    case Mode::kNext: c.next.currentEffect = other.c.next.currentEffect; break;
    case Mode::kLoop: break;
    case Mode::kBrightness: c.brightness.brightness = other.c.brightness.brightness; break;
    case Mode::kSpecial: break;
  }
  return *this;
}

bool AtomS3Ui::DisplayContents::operator==(const DisplayContents& other) const {
  if (mode != other.mode) { return false; }
  switch (mode) {
    case Mode::kUninitialized: return true;
    case Mode::kOff: return true;
    case Mode::kLockedShort: return true;
    case Mode::kLockedLong: return true;
    case Mode::kNext:
      if (c.next.currentEffect != other.c.next.currentEffect) { return false; }
      return true;
    case Mode::kLoop: return true;
    case Mode::kBrightness:
      if (c.brightness.brightness != other.c.brightness.brightness) { return false; }
      return true;
    case Mode::kSpecial: return true;
  }
  return false;
}

}  // namespace jazzlights

#endif  // JL_IS_CONTROLLER(ATOM_S3)
#endif  // ESP32
