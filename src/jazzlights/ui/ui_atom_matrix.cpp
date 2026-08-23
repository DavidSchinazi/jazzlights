#include "jazzlights/ui/ui_atom_matrix.h"

#include "jazzlights/util/config.h"

#if JL_IS_CONTROLLER(ATOM_MATRIX)

#include <cstdint>
#include <memory>
#include <optional>

#include "jazzlights/effect/creatures.h"
#include "jazzlights/network/esp32_ble.h"
#include "jazzlights/network/wifi.h"
#include "jazzlights/render/fastled_runner.h"
#include "jazzlights/render/fastled_wrapper.h"
#include "jazzlights/ui/gpio_button.h"
#include "jazzlights/ui/text.h"

namespace jazzlights {
namespace {

static constexpr uint8_t kButtonPin = 39;
static constexpr Microseconds kButtonLockTimeout = 10000000;                     // 10s.
static constexpr Microseconds kButtonLockTimeoutDuringUnlockSequence = 4000000;  // 4s.

static constexpr uint8_t kBrightnessList[] = {2, 4, 8, 16, 32, 64, 128, 255};
static constexpr uint8_t kNumBrightnesses = sizeof(kBrightnessList) / sizeof(kBrightnessList[0]);

#if JL_DEV && !JL_IS_CONFIG(CLOUDS)
static constexpr uint8_t kInitialBrightnessCursor = 0;
#elif JL_IS_CONFIG(STAFF)
static constexpr uint8_t kInitialBrightnessCursor = 3;
#elif JL_IS_CONFIG(HAMMER) || JL_IS_CONFIG(CLOUDS) || JL_IS_CONFIG(XMAS_TREE) || JL_IS_CONFIG(FAIRY_STRING)
static constexpr uint8_t kInitialBrightnessCursor = 7;
#else
static constexpr uint8_t kInitialBrightnessCursor = 4;
#endif

static const CRGB nextColor = CRGB::Blue;
static const CRGB menuIconNext[ATOM_SCREEN_NUM_LEDS] = {
    nextColor,   CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, nextColor,   nextColor,   CRGB::Black, CRGB::Black,
    CRGB::Black, nextColor,   nextColor,   nextColor,   CRGB::Black, CRGB::Black, nextColor,   nextColor,   CRGB::Black,
    CRGB::Black, CRGB::Black, nextColor,   CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black,
};

static const CRGB prevColor = CRGB::Red;
static const CRGB menuIconPrevious[ATOM_SCREEN_NUM_LEDS] = {
    CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, prevColor,   CRGB::Black, prevColor,
    CRGB::Black, CRGB::Black, prevColor,   CRGB::Black, prevColor,   CRGB::Black, CRGB::Black, prevColor,   CRGB::Black,
    prevColor,   CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black,
};

static const CRGB menuIconBrightness[ATOM_SCREEN_NUM_LEDS] = {
    CRGB::Black,     CRGB::Black,         CRGB::Black, CRGB::Black,         CRGB::Black,     CRGB::DarkGoldenrod,
    CRGB::Goldenrod, CRGB::DarkGoldenrod, CRGB::Black, CRGB::Black,         CRGB::Goldenrod, CRGB::LightGoldenrodYellow,
    CRGB::Goldenrod, CRGB::Black,         CRGB::Black, CRGB::DarkGoldenrod, CRGB::Goldenrod, CRGB::DarkGoldenrod,
    CRGB::Black,     CRGB::Black,         CRGB::Black, CRGB::Black,         CRGB::Black,     CRGB::Black,
    CRGB::Black,
};

static const CRGB menuIconSpecialOff[ATOM_SCREEN_NUM_LEDS] = {
    CRGB::Yellow, CRGB::Yellow, CRGB::Yellow, CRGB::Black, CRGB::Black,  CRGB::Black, CRGB::Black,
    CRGB::Yellow, CRGB::Black,  CRGB::Black,  CRGB::Black, CRGB::Yellow, CRGB::Black, CRGB::Black,
    CRGB::Black,  CRGB::Black,  CRGB::Black,  CRGB::Black, CRGB::Black,  CRGB::Black, CRGB::Black,
    CRGB::Yellow, CRGB::Black,  CRGB::Black,  CRGB::Black,
};
static const CRGB menuIconSpecialCalibration[ATOM_SCREEN_NUM_LEDS] = {
    CRGB::Red,    CRGB::Green, CRGB::Blue,  CRGB::Black, CRGB::Black,  CRGB::Black, CRGB::Black,
    CRGB::Yellow, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Purple, CRGB::Black, CRGB::Black,
    CRGB::Black,  CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black,  CRGB::Black, CRGB::Black,
    CRGB::White,  CRGB::Black, CRGB::Black, CRGB::Black,
};
static const CRGB menuIconSpecialBlack[ATOM_SCREEN_NUM_LEDS] = {
    CRGB::Purple, CRGB::Purple, CRGB::Purple, CRGB::Black, CRGB::Black,  CRGB::Black, CRGB::Black,
    CRGB::Purple, CRGB::Black,  CRGB::Black,  CRGB::Black, CRGB::Purple, CRGB::Black, CRGB::Black,
    CRGB::Black,  CRGB::Black,  CRGB::Black,  CRGB::Black, CRGB::Black,  CRGB::Black, CRGB::Black,
    CRGB::Purple, CRGB::Black,  CRGB::Black,  CRGB::Black,
};
static const CRGB menuIconSpecialRed[ATOM_SCREEN_NUM_LEDS] = {
    CRGB::Red,   CRGB::Red,   CRGB::Red,   CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Red,   CRGB::Black,
    CRGB::Black, CRGB::Black, CRGB::Red,   CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black,
    CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Red,   CRGB::Black, CRGB::Black, CRGB::Black,
};
static const CRGB menuIconSpecialGreen[ATOM_SCREEN_NUM_LEDS] = {
    CRGB::Green, CRGB::Green, CRGB::Green, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Green, CRGB::Black,
    CRGB::Black, CRGB::Black, CRGB::Green, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black,
    CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Green, CRGB::Black, CRGB::Black, CRGB::Black,
};
static const CRGB menuIconSpecialBlue[ATOM_SCREEN_NUM_LEDS] = {
    CRGB::Blue,  CRGB::Blue,  CRGB::Blue,  CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Blue,  CRGB::Black,
    CRGB::Black, CRGB::Black, CRGB::Blue,  CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black,
    CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Blue,  CRGB::Black, CRGB::Black, CRGB::Black,
};
static const CRGB menuIconSpecialWhite[ATOM_SCREEN_NUM_LEDS] = {
    CRGB::White, CRGB::White, CRGB::White, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::White, CRGB::Black,
    CRGB::Black, CRGB::Black, CRGB::White, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black,
    CRGB::Black, CRGB::Black, CRGB::Black, CRGB::White, CRGB::Black, CRGB::Black, CRGB::Black,
};

static const CRGB kAtomScreenLEDsAllZero[ATOM_SCREEN_NUM_LEDS] = {};

uint8_t GetReceiveTimeBrightness(OptionalMicroseconds lastReceiveTime) {
  if (!lastReceiveTime) { return 0; }
  constexpr Microseconds kReceiveMaxTime = 10000 * kMicrosecondsPerMillisecond;
  const Microseconds timeSinceReceive = timeMicros() - *lastReceiveTime;
  if (timeSinceReceive >= kReceiveMaxTime) { return 0; }
  return 255 - static_cast<uint8_t>(timeSinceReceive * 256 / kReceiveMaxTime);
}

}  // namespace

void AtomMatrixUi::ScreenDisplay() {
  // M5Stack recommends not setting the atom screen brightness greater
  // than 20 to avoid melting the screen/cover over the LEDs.
  // Extract bits 16,17,18,19 from microseconds timer to get a value that cycles from 0 to 15 every second
  // For t values 0..7 we subtract that from 20 to get brightness 20..13
  // For t values 8..15 we add that to 4 to get brightness 12..19
  // This gives us a brightness that starts at 20, dims to 12, and then brightens back to 20 every second
  const uint32_t t = (timeMicros() >> 16) & 0xF;
  uint8_t brightness = t & 8 ? 4 + t : 20 - t;
  if (memcmp(screenLEDs_, kAtomScreenLEDsAllZero, sizeof(screenLEDs_)) == 0) { brightness = 0; }
  if (brightness == brightnessLastWrite_ && memcmp(screenLEDs_, screenLEDsLastWrite_, sizeof(screenLEDs_)) == 0) {
    return;
  }
  brightnessLastWrite_ = brightness;
  memcpy(screenLEDsLastWrite_, screenLEDs_, sizeof(screenLEDs_));
  runner_->IngestUiPixels(screenLEDs_, brightness);
}

void AtomMatrixUi::ScreenNetwork() {
  // Change top-right Atom matrix screen LED based on network status.
  CRGB wifiStatusColor = CRGB::Black;
  CRGB followedNetworkColor = CRGB::Red;
#if JL_WIFI
  switch (WiFiNetwork::get()->status()) {
    case INITIALIZING: wifiStatusColor = CRGB::Pink; break;
    case CONNECTING: wifiStatusColor = CRGB::Yellow; break;
    case CONNECTED: wifiStatusColor = CRGB(0, 255, 0); break;
    case CONNECTION_FAILED: wifiStatusColor = CRGB::Red; break;
  }
  if (player_.following() == NetworkType::kWiFi) {
    switch (player_.currentNumHops()) {
      case 1: followedNetworkColor = CRGB(0, 255, 0); break;
      case 2: followedNetworkColor = CRGB(128, 255, 0); break;
      default: followedNetworkColor = CRGB(255, 255, 0); break;
    }
  }
  const uint8_t wifiBrightness = GetReceiveTimeBrightness(WiFiNetwork::get()->getLastReceiveTime());
#else   // JL_WIFI
  const uint8_t wifiBrightness = 0;
#endif  // JL_WIFI
  const uint8_t bleBrightness = GetReceiveTimeBrightness(Esp32BleNetwork::get()->getLastReceiveTime());
  if (player_.following() == NetworkType::kBLE) {
    switch (player_.currentNumHops()) {
      case 1: followedNetworkColor = CRGB(0, 0, 255); break;
      case 2: followedNetworkColor = CRGB(128, 0, 255); break;
      default: followedNetworkColor = CRGB(255, 0, 255); break;
    }
  }
  screenLEDs_[4] = wifiStatusColor;
  screenLEDs_[9] = CRGB(255 - wifiBrightness, wifiBrightness, 0);
  screenLEDs_[14] = CRGB(255 - bleBrightness, 0, bleBrightness);
  screenLEDs_[24] = followedNetworkColor;
}

// ATOM Matrix button map looks like this:
// 00 01 02 03 04
// 05 06 07 08 09
// 10 11 12 13 14
// 15 16 17 18 19
// 20 21 22 23 24

void AtomMatrixUi::ScreenUnlocked() {
  const CRGB* icon = screenLEDs_;
  switch (menuMode_) {
    case MenuMode::kNext: icon = menuIconNext; break;
    case MenuMode::kPrevious: icon = menuIconPrevious; break;
    case MenuMode::kBrightness: icon = menuIconBrightness; break;
    case MenuMode::kSpecial: {
      switch (player_.getSpecial()) {
        case 1: icon = menuIconSpecialCalibration; break;
        case 2: icon = menuIconSpecialBlack; break;
        case 3: icon = menuIconSpecialRed; break;
        case 4: icon = menuIconSpecialGreen; break;
        case 5: icon = menuIconSpecialBlue; break;
        case 6: icon = menuIconSpecialWhite; break;
        default: icon = menuIconSpecialOff; break;
      }
    } break;
  }
  for (int i = 0; i < ATOM_SCREEN_NUM_LEDS; i++) { screenLEDs_[i] = icon[i]; }
  if (menuMode_ == MenuMode::kBrightness) {
    for (int i = 0; i < 8; i++) {
      static const uint8_t brightnessDial[] = {06, 07, 12, 17, 16, 15, 10, 05};
      if (brightnessCursor_ < i) {
        screenLEDs_[brightnessDial[i]] = CRGB::Black;
      } else if (player_.is_power_limited()) {
        screenLEDs_[brightnessDial[i]] = CRGB::Red;
      }
    }
  }
  ScreenNetwork();
}

void AtomMatrixUi::ScreenClear() {
  for (int i = 0; i < ATOM_SCREEN_NUM_LEDS; i++) { screenLEDs_[i] = CRGB::Black; }
}

void AtomMatrixUi::ScreenLong() {
  ScreenClear();
  for (int i : {0, 5, 10, 15, 20, 21, 22}) { screenLEDs_[i] = CRGB::Gold; }
  ScreenNetwork();
}

void AtomMatrixUi::ScreenShort() {
  ScreenClear();
  for (int i : {2, 1, 0, 5, 10, 11, 12, 17, 22, 21, 20}) { screenLEDs_[i] = CRGB::Gold; }
  ScreenNetwork();
}

AtomMatrixUi::AtomMatrixUi(Player& player)
    : Esp32Ui(player), button_(kButtonPin, *this), brightnessCursor_(kInitialBrightnessCursor) {}

bool AtomMatrixUi::IsLocked() {
#if JL_BUTTON_LOCK
  return buttonLockState_ < 5;
#else   // JL_BUTTON_LOCK
  return false;
#endif  // JL_BUTTON_LOCK
}

void AtomMatrixUi::HandleUnlockSequence(bool wasLongPress) {
  if (!IsLocked()) { return; }
  // If we don’t receive the correct button event for the state we’re currently in, return immediately to state 0.
  // In odd states (1,3) we want a long press; in even states (0,2) we want a short press.
  if (((buttonLockState_ % 2) == 1) != wasLongPress) {
    buttonLockState_ = 0;
  } else {
    buttonLockState_++;
    // To reject accidental presses, exit unlock sequence if four seconds without progress
    lockButtonTime_ = timeMicros() + kButtonLockTimeoutDuringUnlockSequence;
  }
}

void AtomMatrixUi::ShortPress(uint8_t pin) {
  if (pin != kButtonPin) { return; }
  jll_info("AtomMatrixUi ShortPress");
  HandleUnlockSequence(/*wasLongPress=*/false);
  if (IsLocked()) { return; }

  // Act on current menu mode.
  switch (menuMode_) {
    case MenuMode::kNext:
      jll_info("Next button has been hit");
      player_.stopSpecial();
      player_.stopLooping();
      player_.next();
      break;
    case MenuMode::kPrevious:
      jll_info("Back button has been hit");
      player_.stopSpecial();
      player_.loopOne();
      break;
    case MenuMode::kBrightness:
      brightnessCursor_ = (brightnessCursor_ + 1 < kNumBrightnesses) ? brightnessCursor_ + 1 : 0;
      jll_info("Brightness button has been hit %u", kBrightnessList[brightnessCursor_]);
      player_.set_brightness(kBrightnessList[brightnessCursor_]);
      break;
    case MenuMode::kSpecial:
      jll_info("Special button has been hit");
      player_.handleSpecial();
      break;
  }
}

void AtomMatrixUi::LongPress(uint8_t pin) {
  if (pin != kButtonPin) { return; }
  jll_info("AtomMatrixUi LongPress");
  HandleUnlockSequence(/*wasLongPress=*/true);
  if (IsLocked()) { return; }

  // Move to next menu mode.
  switch (menuMode_) {
    case MenuMode::kNext: menuMode_ = MenuMode::kPrevious; break;
    case MenuMode::kPrevious: menuMode_ = MenuMode::kBrightness; break;
    case MenuMode::kBrightness: menuMode_ = MenuMode::kNext; break;
    case MenuMode::kSpecial: menuMode_ = MenuMode::kNext; break;
  }
}

void AtomMatrixUi::HeldDown(uint8_t pin) {
  if (pin != kButtonPin) { return; }
  jll_info("AtomMatrixUi HeldDown");
  if (IsLocked()) {
    // Button was held too long, go back to beginning of unlock sequence.
    buttonLockState_ = 0;
    return;
  }
  menuMode_ = MenuMode::kSpecial;
}

bool AtomMatrixUi::ScreenMessage() {
  if (!displayingBootMessage_) { return false; }
  if (button_.IsPressed()) {
    jll_info("Stopping boot message due to button press");
    displayingBootMessage_ = false;
  } else {
    Microseconds currentTime = timeMicros();
    if (!bootMessageStartTime_) { bootMessageStartTime_ = currentTime; }
    Microseconds offsetMicros = currentTime - *bootMessageStartTime_;
#if JL_IS_CONFIG(CREATURE)
    static const CRGB textColor = ThisCreatureColor();
    static constexpr Microseconds kShowColorTime = 3000000;
    if (offsetMicros <= kShowColorTime) {
      // Show this creature's color for `kShowColorTime` after boot.
      for (int i = 0; i < ATOM_SCREEN_NUM_LEDS; i++) { screenLEDs_[i] = textColor; }
      ScreenDisplay();
      return true;
    }
    offsetMicros -= kShowColorTime;
#else   // CREATURE
    static const CRGB textColor = CRGB::Red;
#endif  // CREATURE
    displayingBootMessage_ = displayText(BOOT_MESSAGE, screenLEDs_, textColor, CRGB::Black, offsetMicros);
    if (!displayingBootMessage_) {
      jll_info("Done displaying boot message");
    } else {
      ScreenDisplay();
    }
  }
  return displayingBootMessage_;
}

void AtomMatrixUi::RunLoop() {
  Microseconds currentTime = timeMicros();
  button_.RunLoop();

  if (ScreenMessage()) { return; }

#if JL_IS_CONFIG(FAIRY_WAND)
  ScreenClear();
  ScreenDisplay();
  if (button_.IsPressed()) { player_.triggerPatternOverride(); }
  return;
#endif  // FAIRY_WAND

#if JL_BUTTON_LOCK

  // If idle-time expired, return to ‘locked’ state
  if (buttonLockState_ != 0 && lockButtonTime_ && currentTime - *lockButtonTime_ >= 0) {
    jll_info("Locking buttons");
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
      ScreenClear();
    } else if ((buttonLockState_ % 2) == 1) {
      // In odd  states (1,3) we show "L".
      ScreenLong();
    } else {
      // In even states (0,2) we show "S".
      ScreenShort();
    }
    ScreenDisplay();

    // In lock state 4, wait for release of the button, and then move to state 5 (fully unlocked)
    if (buttonLockState_ < 4 || button_.IsPressed()) { return; }
    buttonLockState_ = 5;
    lockButtonTime_ = currentTime + kButtonLockTimeout;
  } else if (button_.IsPressed()) {
    lockButtonTime_ = currentTime + kButtonLockTimeout;
  }
#endif  // JL_BUTTON_LOCK
  ScreenUnlocked();
  ScreenDisplay();
}

void AtomMatrixUi::InitialSetup() {
  runner_->ConfigureUi<WS2812, /*DATA_PIN=*/27, GRB>(ATOM_SCREEN_NUM_LEDS);
  ScreenClear();
  player_.set_brightness(kBrightnessList[brightnessCursor_]);
}

void AtomMatrixUi::FinalSetup() {}

}  // namespace jazzlights

#endif
