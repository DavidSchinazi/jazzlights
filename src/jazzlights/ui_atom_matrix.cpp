#include "jazzlights/ui.h"
// This comment prevents automatic header reordering.

#include "jazzlights/config.h"

#if JL_IS_CONTROLLER(ATOM_MATRIX)

#include <Arduino.h>

#include <cstdint>
#include <memory>

#include "jazzlights/fastled_wrapper.h"
#include "jazzlights/gpio_button.h"
#include "jazzlights/networks/arduino_esp_wifi.h"
#include "jazzlights/networks/esp32_ble.h"
#include "jazzlights/text.h"

namespace jazzlights {
namespace {

#define ATOM_SCREEN_NUM_LEDS 25
CRGB atomScreenLEDs[ATOM_SCREEN_NUM_LEDS] = {};
CRGB atomScreenLEDsLastWrite[ATOM_SCREEN_NUM_LEDS] = {};
CRGB atomScreenLEDsAllZero[ATOM_SCREEN_NUM_LEDS] = {};
uint8_t brightnessLastWrite = 255;

static constexpr Milliseconds kButtonLockTimeout = 10000;                     // 10s.
static constexpr Milliseconds kButtonLockTimeoutDuringUnlockSequence = 4000;  // 4s.

const uint8_t brightnessList[] = {2, 4, 8, 16, 32, 64, 128, 255};
#define NUM_BRIGHTNESSES (sizeof(brightnessList) / sizeof(brightnessList[0]))
#define MAX_BRIGHTNESS (NUM_BRIGHTNESSES - 1)

#if JL_DEV
static constexpr uint8_t kInitialBrightnessCursor = 0;
#elif JL_IS_CONFIG(STAFF)
static constexpr uint8_t kInitialBrightnessCursor = 3;
#elif JL_IS_CONFIG(HAMMER)
static constexpr uint8_t kInitialBrightnessCursor = 7;
#else
static constexpr uint8_t kInitialBrightnessCursor = 4;
#endif

uint8_t brightnessCursor = kInitialBrightnessCursor;

enum atomMenuMode {
  kNext,
  kPrevious,
  kBrightness,
  kSpecial,
};

atomMenuMode menuMode = kNext;

void nextMode(Player& /*player*/, const Milliseconds /*currentMillis*/) {
  switch (menuMode) {
    case kNext: menuMode = kPrevious; break;
    case kPrevious: menuMode = kBrightness; break;
    case kBrightness: menuMode = kNext; break;
    case kSpecial: menuMode = kNext; break;
  };
}

void modeAct(Player& player, const Milliseconds currentMillis) {
  switch (menuMode) {
    case kNext:
      jll_info("%u Next button has been hit", currentMillis);
      player.stopSpecial(currentMillis);
      player.stopLooping(currentMillis);
      player.next(currentMillis);
      break;
    case kPrevious:
      jll_info("%u Back button has been hit", currentMillis);
      player.stopSpecial(currentMillis);
      player.loopOne(currentMillis);
      break;
    case kBrightness:
      brightnessCursor = (brightnessCursor + 1 < NUM_BRIGHTNESSES) ? brightnessCursor + 1 : 0;
      jll_info("%u Brightness button has been hit %u", currentMillis, brightnessList[brightnessCursor]);
      break;
    case kSpecial:
      jll_info("%u Special button has been hit", currentMillis);
      player.handleSpecial(currentMillis);
      break;
  };
}

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

CLEDController* atomMatrixScreenController = nullptr;

void atomScreenDisplay(const Milliseconds currentMillis) {
  // M5Stack recommends not setting the atom screen brightness greater
  // than 20 to avoid melting the screen/cover over the LEDs.
  // Extract bits 6,7,8,9 from milliseconds timer to get a value that cycles from 0 to 15 every second
  // For t values 0..7 we subtract that from 20 to get brightness 20..13
  // For t values 8..15 we add that to 4 to get brightness 12..19
  // This gives us a brightness that starts at 20, dims to 12, and then brightens back to 20 every second
  const uint32_t t = (currentMillis >> 6) & 0xF;
  uint8_t brightness = t & 8 ? 4 + t : 20 - t;
  if (memcmp(atomScreenLEDs, atomScreenLEDsAllZero, sizeof(atomScreenLEDs)) == 0) { brightness = 0; }
  if (brightness == brightnessLastWrite &&
      memcmp(atomScreenLEDs, atomScreenLEDsLastWrite, sizeof(atomScreenLEDs)) == 0) {
    return;
  }
  brightnessLastWrite = brightness;
  memcpy(atomScreenLEDsLastWrite, atomScreenLEDs, sizeof(atomScreenLEDs));
  atomMatrixScreenController->showLeds(brightness);
}

uint8_t getReceiveTimeBrightness(Milliseconds lastReceiveTime, Milliseconds currentMillis) {
  if (lastReceiveTime < 0) { return 0; }
  constexpr Milliseconds kReceiveMaxTime = 10000;
  const Milliseconds timeSinceReceive = currentMillis - lastReceiveTime;
  if (timeSinceReceive >= kReceiveMaxTime) { return 0; }
  return 255 - static_cast<uint8_t>(timeSinceReceive * 256 / kReceiveMaxTime);
}

void atomScreenNetwork(Player& player, Milliseconds currentMillis) {
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
  atomScreenLEDs[4] = wifiStatusColor;
  CRGB followedNetworkColor = CRGB::Red;
  if (player.followedNextHopNetwork() == ArduinoEspWiFiNetwork::get()) {
    switch (player.currentNumHops()) {
      case 1: followedNetworkColor = CRGB(0, 255, 0); break;
      case 2: followedNetworkColor = CRGB(128, 255, 0); break;
      default: followedNetworkColor = CRGB(255, 255, 0); break;
    }
  }
  const uint8_t wifiBrightness =
      getReceiveTimeBrightness(ArduinoEspWiFiNetwork::get()->getLastReceiveTime(), currentMillis);
  atomScreenLEDs[9] = CRGB(255 - wifiBrightness, wifiBrightness, 0);
  const uint8_t bleBrightness = getReceiveTimeBrightness(Esp32BleNetwork::get()->getLastReceiveTime(), currentMillis);
  atomScreenLEDs[14] = CRGB(255 - bleBrightness, 0, bleBrightness);
  if (player.followedNextHopNetwork() == Esp32BleNetwork::get()) {
    switch (player.currentNumHops()) {
      case 1: followedNetworkColor = CRGB(0, 0, 255); break;
      case 2: followedNetworkColor = CRGB(128, 0, 255); break;
      default: followedNetworkColor = CRGB(255, 0, 255); break;
    }
  }
  atomScreenLEDs[24] = followedNetworkColor;
}

// ATOM Matrix button map looks like this:
// 00 01 02 03 04
// 05 06 07 08 09
// 10 11 12 13 14
// 15 16 17 18 19
// 20 21 22 23 24

void atomScreenUnlocked(Player& player, Milliseconds currentMillis) {
  const CRGB* icon = atomScreenLEDs;
  switch (menuMode) {
    case kNext: icon = menuIconNext; break;
    case kPrevious: icon = menuIconPrevious; break;
    case kBrightness: icon = menuIconBrightness; break;
    case kSpecial: {
      switch (player.getSpecial()) {
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
  for (int i = 0; i < ATOM_SCREEN_NUM_LEDS; i++) { atomScreenLEDs[i] = icon[i]; }
  if (menuMode == kBrightness) {
    for (int i = 0; i < 8; i++) {
      static const uint8_t brightnessDial[] = {06, 07, 12, 17, 16, 15, 10, 05};
      if (brightnessCursor < i) {
        atomScreenLEDs[brightnessDial[i]] = CRGB::Black;
      } else if (player.powerLimited) {
        atomScreenLEDs[brightnessDial[i]] = CRGB::Red;
      }
    }
  }
  atomScreenNetwork(player, currentMillis);
}

void atomScreenClear() {
  for (int i = 0; i < ATOM_SCREEN_NUM_LEDS; i++) { atomScreenLEDs[i] = CRGB::Black; }
}

void atomScreenLong(Player& player, Milliseconds currentMillis) {
  atomScreenClear();
  for (int i : {0, 5, 10, 15, 20, 21, 22}) { atomScreenLEDs[i] = CRGB::Gold; }
  atomScreenNetwork(player, currentMillis);
}

void atomScreenShort(Player& player, Milliseconds currentMillis) {
  atomScreenClear();
  for (int i : {2, 1, 0, 5, 10, 11, 12, 17, 22, 21, 20}) { atomScreenLEDs[i] = CRGB::Gold; }
  atomScreenNetwork(player, currentMillis);
}

static constexpr uint8_t kButtonPin = 39;

class AtomMatrixUI : public Button::Interface {
 public:
  explicit AtomMatrixUI(Player& player, Milliseconds currentTime)
      : player_(player), button_(kButtonPin, *this, currentTime) {}
  bool IsLocked() { return buttonLockState_ < 5; }
  void HandleUnlockSequence(bool wasLongPress, Milliseconds currentMillis) {
    if (!IsLocked()) { return; }
    // If we don’t receive the correct button event for the state we’re currently in, return immediately to state 0.
    // In odd states (1,3) we want a long press; in even states (0,2) we want a short press.
    if (((buttonLockState_ % 2) == 1) != wasLongPress) {
      buttonLockState_ = 0;
    } else {
      buttonLockState_++;
      // To reject accidental presses, exit unlock sequence if four seconds without progress
      lockButtonTime_ = currentMillis + kButtonLockTimeoutDuringUnlockSequence;
    }
  }
  void ShortPress(uint8_t pin, Milliseconds currentMillis) override {
    if (pin != kButtonPin) { return; }
    jll_info("%u ShortPress", currentMillis);
    HandleUnlockSequence(/*wasLongPress=*/false, currentMillis);
    if (IsLocked()) { return; }

    modeAct(player_, currentMillis);
  }
  void LongPress(uint8_t pin, Milliseconds currentMillis) override {
    if (pin != kButtonPin) { return; }
    jll_info("%u LongPress", currentMillis);
    HandleUnlockSequence(/*wasLongPress=*/true, currentMillis);
    if (IsLocked()) { return; }

    nextMode(player_, currentMillis);
  }
  void HeldDown(uint8_t pin, Milliseconds currentMillis) override {
    if (pin != kButtonPin) { return; }
    jll_info("%u HeldDown", currentMillis);
    if (IsLocked()) {
      // Button was held too long, go back to beginning of unlock sequence.
      buttonLockState_ = 0;
      return;
    }
    menuMode = kSpecial;
  }

  bool atomScreenMessage(const Milliseconds currentMillis) {
    if (!displayingBootMessage_) { return false; }
    if (button_.IsPressed(currentMillis)) {
      jll_info("%u Stopping boot message due to button press", currentMillis);
      displayingBootMessage_ = false;
    } else {
      static Milliseconds bootMessageStartTime = -1;
      if (bootMessageStartTime < 0) { bootMessageStartTime = currentMillis; }
      displayingBootMessage_ =
          displayText(BOOT_MESSAGE, atomScreenLEDs, CRGB::Red, CRGB::Black, currentMillis - bootMessageStartTime);
      if (!displayingBootMessage_) {
        jll_info("%u Done displaying boot message", currentMillis);
      } else {
        atomScreenDisplay(currentMillis);
      }
    }
    return displayingBootMessage_;
  }

  void RunLoop(Milliseconds currentMillis) {
    button_.RunLoop(currentMillis);

    if (atomScreenMessage(currentMillis)) { return; }

#if JL_IS_CONFIG(FAIRY_WAND)
    atomScreenClear();
    atomScreenDisplay(currentMillis);
    if (BTN_EVENT(btn)) { player.triggerPatternOverride(currentMillis); }
    return;
#endif  // FAIRY_WAND

#if JL_BUTTON_LOCK

    // If idle-time expired, return to ‘locked’ state
    if (buttonLockState_ != 0 && currentMillis - lockButtonTime_ >= 0) {
      jll_info("%u Locking buttons", currentMillis);
      buttonLockState_ = 0;
    }

    if (IsLocked()) {
      // We show a blank display:
      // 1. When in fully locked mode, with the button not pressed
      // 2. When the button has been pressed long enough to register as a long press, and we want to signal the user to
      // let go now
      // 3. In the final transition from state 4 (awaiting release) to state 5 (unlocked)
      if ((buttonLockState_ == 0 && !button_.IsPressed(currentMillis)) ||
          button_.HasBeenPressedLongEnoughForLongPress(currentMillis) || buttonLockState_ >= 4) {
        atomScreenClear();
      } else if ((buttonLockState_ % 2) == 1) {
        // In odd  states (1,3) we show "L".
        atomScreenLong(player_, currentMillis);
      } else {
        // In even states (0,2) we show "S".
        atomScreenShort(player_, currentMillis);
      }
      atomScreenDisplay(currentMillis);

      // In lock state 4, wait for release of the button, and then move to state 5 (fully unlocked)
      if (buttonLockState_ < 4 || button_.IsPressed(currentMillis)) { return; }
      buttonLockState_ = 5;
      lockButtonTime_ = currentMillis + kButtonLockTimeout;
    } else if (button_.IsPressed(currentMillis)) {
      lockButtonTime_ = currentMillis + kButtonLockTimeout;
    }
#endif  // JL_BUTTON_LOCK
    atomScreenUnlocked(player_, currentMillis);
    atomScreenDisplay(currentMillis);
  }

 private:
  Player& player_;
  Button button_;
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

std::unique_ptr<AtomMatrixUI> gUi;
}  // namespace

void arduinoUiInitialSetup(Player& player, Milliseconds currentTime) {
  gUi.reset(new AtomMatrixUI(player, currentTime));
  atomMatrixScreenController = &FastLED.addLeds<WS2812, /*DATA_PIN=*/27, GRB>(atomScreenLEDs, ATOM_SCREEN_NUM_LEDS);
  atomScreenClear();
}

void arduinoUiFinalSetup(Player& /*player*/, Milliseconds /*currentTime*/) {}

void arduinoUiLoop(Player& /*player*/, Milliseconds currentMillis) { gUi->RunLoop(currentMillis); }

uint8_t getBrightness() { return brightnessList[brightnessCursor]; }

}  // namespace jazzlights

#endif
