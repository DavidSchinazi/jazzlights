#include "jazzlights/ui/ui_atom_matrix.h"

#include "jazzlights/config.h"

#if JL_IS_CONTROLLER(ATOM_MATRIX)

#include <Arduino.h>

#include <cstdint>
#include <memory>

#include "jazzlights/fastled_wrapper.h"
#include "jazzlights/networks/arduino_esp_wifi.h"
#include "jazzlights/networks/esp32_ble.h"
#include "jazzlights/text.h"
#include "jazzlights/ui/gpio_button.h"

namespace jazzlights {
namespace {

static constexpr uint8_t kButtonPin = 39;
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

uint8_t GetReceiveTimeBrightness(Milliseconds lastReceiveTime, Milliseconds currentTime) {
  if (lastReceiveTime < 0) { return 0; }
  constexpr Milliseconds kReceiveMaxTime = 10000;
  const Milliseconds timeSinceReceive = currentTime - lastReceiveTime;
  if (timeSinceReceive >= kReceiveMaxTime) { return 0; }
  return 255 - static_cast<uint8_t>(timeSinceReceive * 256 / kReceiveMaxTime);
}

}  // namespace

void AtomMatrixUi::ScreenDisplay(Milliseconds currentTime) {
  // M5Stack recommends not setting the atom screen brightness greater
  // than 20 to avoid melting the screen/cover over the LEDs.
  // Extract bits 6,7,8,9 from milliseconds timer to get a value that cycles from 0 to 15 every second
  // For t values 0..7 we subtract that from 20 to get brightness 20..13
  // For t values 8..15 we add that to 4 to get brightness 12..19
  // This gives us a brightness that starts at 20, dims to 12, and then brightens back to 20 every second
  const uint32_t t = (currentTime >> 6) & 0xF;
  uint8_t brightness = t & 8 ? 4 + t : 20 - t;
  if (memcmp(screenLEDs_, kAtomScreenLEDsAllZero, sizeof(screenLEDs_)) == 0) { brightness = 0; }
  if (brightness == brightnessLastWrite_ && memcmp(screenLEDs_, screenLEDsLastWrite_, sizeof(screenLEDs_)) == 0) {
    return;
  }
  brightnessLastWrite_ = brightness;
  memcpy(screenLEDsLastWrite_, screenLEDs_, sizeof(screenLEDs_));
  screenController_->showLeds(brightness);
}

void AtomMatrixUi::ScreenNetwork(Milliseconds currentTime) {
  // Change top-right Atom matrix screen LED based on network status.
  CRGB wifiStatusColor = CRGB::Black;
  switch (ArduinoEspWiFiNetwork::get()->status()) {
    case INITIALIZING: wifiStatusColor = CRGB::Pink; break;
    case DISCONNECTED: wifiStatusColor = CRGB::Purple; break;
    case CONNECTING: wifiStatusColor = CRGB::Yellow; break;
    case CONNECTED: wifiStatusColor = CRGB(0, 255, 0); break;
    case DISCONNECTING: wifiStatusColor = CRGB::Orange; break;
    case CONNECTION_FAILED: wifiStatusColor = CRGB::Red; break;
  }
  screenLEDs_[4] = wifiStatusColor;
  CRGB followedNetworkColor = CRGB::Red;
  if (player_.followedNextHopNetwork() == ArduinoEspWiFiNetwork::get()) {
    switch (player_.currentNumHops()) {
      case 1: followedNetworkColor = CRGB(0, 255, 0); break;
      case 2: followedNetworkColor = CRGB(128, 255, 0); break;
      default: followedNetworkColor = CRGB(255, 255, 0); break;
    }
  }
  const uint8_t wifiBrightness =
      GetReceiveTimeBrightness(ArduinoEspWiFiNetwork::get()->getLastReceiveTime(), currentTime);
  screenLEDs_[9] = CRGB(255 - wifiBrightness, wifiBrightness, 0);
  const uint8_t bleBrightness = GetReceiveTimeBrightness(Esp32BleNetwork::get()->getLastReceiveTime(), currentTime);
  screenLEDs_[14] = CRGB(255 - bleBrightness, 0, bleBrightness);
  if (player_.followedNextHopNetwork() == Esp32BleNetwork::get()) {
    switch (player_.currentNumHops()) {
      case 1: followedNetworkColor = CRGB(0, 0, 255); break;
      case 2: followedNetworkColor = CRGB(128, 0, 255); break;
      default: followedNetworkColor = CRGB(255, 0, 255); break;
    }
  }
  screenLEDs_[24] = followedNetworkColor;
}

// ATOM Matrix button map looks like this:
// 00 01 02 03 04
// 05 06 07 08 09
// 10 11 12 13 14
// 15 16 17 18 19
// 20 21 22 23 24

void AtomMatrixUi::ScreenUnlocked(Milliseconds currentTime) {
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
  };
  for (int i = 0; i < ATOM_SCREEN_NUM_LEDS; i++) { screenLEDs_[i] = icon[i]; }
  if (menuMode_ == MenuMode::kBrightness) {
    for (int i = 0; i < 8; i++) {
      static const uint8_t brightnessDial[] = {06, 07, 12, 17, 16, 15, 10, 05};
      if (brightnessCursor_ < i) {
        screenLEDs_[brightnessDial[i]] = CRGB::Black;
      } else if (player_.IsPowerLimited()) {
        screenLEDs_[brightnessDial[i]] = CRGB::Red;
      }
    }
  }
  ScreenNetwork(currentTime);
}

void AtomMatrixUi::ScreenClear(Milliseconds /*currentTime*/) {
  for (int i = 0; i < ATOM_SCREEN_NUM_LEDS; i++) { screenLEDs_[i] = CRGB::Black; }
}

void AtomMatrixUi::ScreenLong(Milliseconds currentTime) {
  ScreenClear(currentTime);
  for (int i : {0, 5, 10, 15, 20, 21, 22}) { screenLEDs_[i] = CRGB::Gold; }
  ScreenNetwork(currentTime);
}

void AtomMatrixUi::ScreenShort(Milliseconds currentTime) {
  ScreenClear(currentTime);
  for (int i : {2, 1, 0, 5, 10, 11, 12, 17, 22, 21, 20}) { screenLEDs_[i] = CRGB::Gold; }
  ScreenNetwork(currentTime);
}

AtomMatrixUi::AtomMatrixUi(Player& player, Milliseconds currentTime)
    : ArduinoUi(player, currentTime),
      button_(kButtonPin, *this, currentTime),
      brightnessCursor_(kInitialBrightnessCursor) {}

bool AtomMatrixUi::IsLocked() { return buttonLockState_ < 5; }

void AtomMatrixUi::HandleUnlockSequence(bool wasLongPress, Milliseconds currentTime) {
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

void AtomMatrixUi::ShortPress(uint8_t pin, Milliseconds currentTime) {
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
  };
}

void AtomMatrixUi::LongPress(uint8_t pin, Milliseconds currentTime) {
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
  };
}

void AtomMatrixUi::HeldDown(uint8_t pin, Milliseconds currentTime) {
  if (pin != kButtonPin) { return; }
  jll_info("%u HeldDown", currentTime);
  if (IsLocked()) {
    // Button was held too long, go back to beginning of unlock sequence.
    buttonLockState_ = 0;
    return;
  }
  menuMode_ = MenuMode::kSpecial;
}

bool AtomMatrixUi::ScreenMessage(const Milliseconds currentTime) {
  if (!displayingBootMessage_) { return false; }
  if (button_.IsPressed(currentTime)) {
    jll_info("%u Stopping boot message due to button press", currentTime);
    displayingBootMessage_ = false;
  } else {
    static Milliseconds bootMessageStartTime = -1;
    if (bootMessageStartTime < 0) { bootMessageStartTime = currentTime; }
    displayingBootMessage_ =
        displayText(BOOT_MESSAGE, screenLEDs_, CRGB::Red, CRGB::Black, currentTime - bootMessageStartTime);
    if (!displayingBootMessage_) {
      jll_info("%u Done displaying boot message", currentTime);
    } else {
      ScreenDisplay(currentTime);
    }
  }
  return displayingBootMessage_;
}

void AtomMatrixUi::RunLoop(Milliseconds currentTime) {
  button_.RunLoop(currentTime);

  if (ScreenMessage(currentTime)) { return; }

#if JL_IS_CONFIG(FAIRY_WAND)
  atomScreenClear();
  ScreenDisplay(currentTime);
  if (BTN_EVENT(btn)) { player.triggerPatternOverride(currentTime); }
  return;
#endif  // FAIRY_WAND

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
      ScreenClear(currentTime);
    } else if ((buttonLockState_ % 2) == 1) {
      // In odd  states (1,3) we show "L".
      ScreenLong(currentTime);
    } else {
      // In even states (0,2) we show "S".
      ScreenShort(currentTime);
    }
    ScreenDisplay(currentTime);

    // In lock state 4, wait for release of the button, and then move to state 5 (fully unlocked)
    if (buttonLockState_ < 4 || button_.IsPressed(currentTime)) { return; }
    buttonLockState_ = 5;
    lockButtonTime_ = currentTime + kButtonLockTimeout;
  } else if (button_.IsPressed(currentTime)) {
    lockButtonTime_ = currentTime + kButtonLockTimeout;
  }
#endif  // JL_BUTTON_LOCK
  ScreenUnlocked(currentTime);
  ScreenDisplay(currentTime);
}

void AtomMatrixUi::InitialSetup(Milliseconds currentTime) {
  screenController_ = &FastLED.addLeds<WS2812, /*DATA_PIN=*/27, GRB>(screenLEDs_, ATOM_SCREEN_NUM_LEDS);
  ScreenClear(currentTime);
  player_.SetBrightness(kBrightnessList[brightnessCursor_]);
}

void AtomMatrixUi::FinalSetup(Milliseconds /*currentTime*/) {}

}  // namespace jazzlights

#endif
