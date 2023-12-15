#include "jazzlights/ui.h"
// This comment prevents automatic header reordering.

#include "jazzlights/config.h"

#if JL_IS_CONTROLLER(ATOM_MATRIX) || JL_IS_CONTROLLER(ATOM_LITE) || JL_IS_CONTROLLER(M5STAMP_PICO) || \
    JL_IS_CONTROLLER(M5STAMP_C3U) || JL_IS_CONTROLLER(ATOM_S3)

#include <Arduino.h>

#include <cstdint>

#include "jazzlights/fastled_wrapper.h"
#include "jazzlights/text.h"

namespace jazzlights {

#ifndef JL_BUTTONS_DISABLED
#define JL_BUTTONS_DISABLED 0
#endif  // JL_BUTTONS_DISABLED

#if ATOM_MATRIX_SCREEN
#define ATOM_SCREEN_NUM_LEDS 25
CRGB atomScreenLEDs[ATOM_SCREEN_NUM_LEDS] = {};
CRGB atomScreenLEDsLastWrite[ATOM_SCREEN_NUM_LEDS] = {};
CRGB atomScreenLEDsAllZero[ATOM_SCREEN_NUM_LEDS] = {};
uint8_t brightnessLastWrite = 255;
#endif  // ATOM_MATRIX_SCREEN

#define NUMBUTTONS 1
uint8_t buttonPins[NUMBUTTONS] = {39};

// Button state tranitions:
// Button starts in BTN_IDLE.
// BTN_IDLE: When pressed, goes to BTN_DEBOUNCING state for BTN_DEBOUNCETIME (20 ms).
// BTN_DEBOUNCING: After BTN_DEBOUNCETIME move to BTN_PRESSED state.
// BTN_PRESSED: If button no longer pressed, go to BTN_RELEASED state
// else if button still held after BTN_LONGPRESSTIME go to BTN_LONGPRESS state.
// BTN_RELEASED: After client consumes BTN_RELEASED event, state retuns to BTN_IDLE.
// BTN_LONGPRESS: After client consumes BTN_LONGPRESS event,
// if button no longer pressed, return to BTN_IDLE state
// else if button still held go to BTN_HOLDING state,
// and if button still held after another BTN_LONGPRESSTIME,
// set button event time to now and return a BTN_LONGLONGPRESS
// to the client, remaining in BTN_HOLDING state.
// This causes the BTN_LONGLONGPRESS events to autorepeat at intervals of
// BTN_LONGPRESSTIME for as long as the button continues to be held.
//
// Note that the BTN_PRESSED state is transitory.
// The client is not guaranteed to be informed of the BTN_PRESSED state if
// the button moves quickly to BTN_RELEASED state before the client notices.
// This is fine, since clients are supposed to act on the BTN_RELEASED state, not BTN_PRESSED.

#define BTN_IDLE 0
#define BTN_DEBOUNCING 1
#define BTN_PRESSED 2
#define BTN_RELEASED 3
#define BTN_LONGPRESS 4
#define BTN_HOLDING 5
#define BTN_LONGLONGPRESS 6

// BTN_IDLE, BTN_DEBOUNCING, BTN_PRESSED, BTN_HOLDING are states, which will be returned continuously for as long as
// that state remains BTN_RELEASED, BTN_LONGPRESS, BTN_LONGLONGPRESS are one-shot events, which will be delivered just
// once each time they occur
#define BTN_STATE(X) ((X) == BTN_IDLE || (X) == BTN_DEBOUNCING || (X) == BTN_PRESSED || (X) == BTN_HOLDING)
#define BTN_EVENT(X) ((X) == BTN_RELEASED || (X) == BTN_LONGPRESS || (X) == BTN_LONGLONGPRESS)
// BTN_SHORT_PRESS_COMPLETE indicates states where a short press is over -- either
// because the button was released, or the button is still held and has transitioned
// into one of the long-press states (BTN_LONGPRESS, BTN_HOLDING, BTN_LONGLONGPRESS)
#define BTN_SHORT_PRESS_COMPLETE(X) ((X) >= BTN_RELEASED)

#define BTN_DEBOUNCETIME 20
#define BTN_LONGPRESSTIME 1000

Milliseconds buttonEvents[NUMBUTTONS];
uint8_t buttonStatuses[NUMBUTTONS];

static constexpr Milliseconds lockDelay = 10000;  // Ten seconds

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

#if ATOM_MATRIX_SCREEN

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

void atomScreenNetwork(Player& player, const Network& wifiNetwork, const Network& bleNetwork,
                       Milliseconds currentMillis) {
  // Change top-right Atom matrix screen LED based on network status.
  CRGB wifiStatusColor = CRGB::Black;
  switch (wifiNetwork.status()) {
    case INITIALIZING: wifiStatusColor = CRGB::Pink; break;
    case DISCONNECTED: wifiStatusColor = CRGB::Purple; break;
    case CONNECTING: wifiStatusColor = CRGB::Yellow; break;
    case CONNECTED: wifiStatusColor = CRGB(0, 255, 0); break;
    case DISCONNECTING: wifiStatusColor = CRGB::Orange; break;
    case CONNECTION_FAILED: wifiStatusColor = CRGB::Red; break;
  }
  atomScreenLEDs[4] = wifiStatusColor;
  CRGB followedNetworkColor = CRGB::Red;
  if (player.followedNextHopNetwork() == &wifiNetwork) {
    switch (player.currentNumHops()) {
      case 1: followedNetworkColor = CRGB(0, 255, 0); break;
      case 2: followedNetworkColor = CRGB(128, 255, 0); break;
      default: followedNetworkColor = CRGB(255, 255, 0); break;
    }
  }
  const uint8_t wifiBrightness = getReceiveTimeBrightness(wifiNetwork.getLastReceiveTime(), currentMillis);
  atomScreenLEDs[9] = CRGB(255 - wifiBrightness, wifiBrightness, 0);
  const uint8_t bleBrightness = getReceiveTimeBrightness(bleNetwork.getLastReceiveTime(), currentMillis);
  atomScreenLEDs[14] = CRGB(255 - bleBrightness, 0, bleBrightness);
  if (player.followedNextHopNetwork() == &bleNetwork) {
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

void atomScreenUnlocked(Player& player, const Network& wifiNetwork, const Network& bleNetwork,
                        Milliseconds currentMillis) {
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
  atomScreenNetwork(player, wifiNetwork, bleNetwork, currentMillis);
}

void atomScreenClear() {
  for (int i = 0; i < ATOM_SCREEN_NUM_LEDS; i++) { atomScreenLEDs[i] = CRGB::Black; }
}

void atomScreenLong(Player& player, const Network& wifiNetwork, const Network& bleNetwork, Milliseconds currentMillis) {
  atomScreenClear();
  for (int i : {0, 5, 10, 15, 20, 21, 22}) { atomScreenLEDs[i] = CRGB::Gold; }
  atomScreenNetwork(player, wifiNetwork, bleNetwork, currentMillis);
}

void atomScreenShort(Player& player, const Network& wifiNetwork, const Network& bleNetwork,
                     Milliseconds currentMillis) {
  atomScreenClear();
  for (int i : {2, 1, 0, 5, 10, 11, 12, 17, 22, 21, 20}) { atomScreenLEDs[i] = CRGB::Gold; }
  atomScreenNetwork(player, wifiNetwork, bleNetwork, currentMillis);
}

bool atomScreenMessage(uint8_t btn, const Milliseconds currentMillis) {
  static bool displayingBootMessage = true;
  if (!displayingBootMessage) { return false; }
  if (btn != BTN_IDLE) {
    displayingBootMessage = false;
    jll_info("%u Stopping boot message due to button press", currentMillis);
    return false;
  }
  static Milliseconds bootMessageStartTime = -1;
  if (bootMessageStartTime < 0) { bootMessageStartTime = currentMillis; }
  displayingBootMessage =
      displayText(BOOT_MESSAGE, atomScreenLEDs, CRGB::Red, CRGB::Black, currentMillis - bootMessageStartTime);
  if (!displayingBootMessage) {
    jll_info("%u Done displaying boot message", currentMillis);
  } else {
    atomScreenDisplay(currentMillis);
  }
  return displayingBootMessage;
}

#endif  // ATOM_MATRIX_SCREEN

void updateButtons(const Milliseconds currentMillis) {
  for (int i = 0; i < NUMBUTTONS; i++) {
    switch (buttonStatuses[i]) {
      case BTN_IDLE:
        if (digitalRead(buttonPins[i]) == LOW) {
          buttonEvents[i] = currentMillis;
          buttonStatuses[i] = BTN_DEBOUNCING;
        }
        break;

      case BTN_DEBOUNCING:
        if (currentMillis - buttonEvents[i] >= BTN_DEBOUNCETIME) { buttonStatuses[i] = BTN_PRESSED; }
        break;

      case BTN_PRESSED:
        if (digitalRead(buttonPins[i]) == HIGH) {
          buttonStatuses[i] = BTN_RELEASED;
        } else if (currentMillis - buttonEvents[i] >= BTN_LONGPRESSTIME) {
          buttonEvents[i] = currentMillis;  // Record the time that we decided to count this as a long press
          buttonStatuses[i] = BTN_LONGPRESS;
        }
        break;

      case BTN_RELEASED: break;

      case BTN_LONGPRESS: break;
    }
  }
}

uint8_t buttonStatus(uint8_t buttonNum, const Milliseconds currentMillis) {
  uint8_t tempStatus = buttonStatuses[buttonNum];
  if (tempStatus == BTN_RELEASED) {
    buttonStatuses[buttonNum] = BTN_IDLE;
  } else if (tempStatus >= BTN_LONGPRESS) {
    if (digitalRead(buttonPins[buttonNum]) == HIGH) {
      buttonStatuses[buttonNum] = BTN_IDLE;
    } else if (currentMillis - buttonEvents[buttonNum] < BTN_LONGPRESSTIME) {
      buttonStatuses[buttonNum] = BTN_HOLDING;
    } else {
      buttonEvents[buttonNum] = currentMillis;
      tempStatus = BTN_LONGLONGPRESS;
    }
  }
  return tempStatus;
}

void arduinoUiInitialSetup(Player& /*player*/, Milliseconds /*currentTime*/) {
  for (uint8_t i = 0; i < sizeof(buttonPins) / sizeof(buttonPins[0]); i++) { pinMode(buttonPins[i], INPUT_PULLUP); }
#if ATOM_MATRIX_SCREEN
  atomMatrixScreenController = &FastLED.addLeds<WS2812, /*DATA_PIN=*/27, GRB>(atomScreenLEDs, ATOM_SCREEN_NUM_LEDS);
  atomScreenClear();
#endif  // ATOM_MATRIX_SCREEN
}

void arduinoUiFinalSetup(Player& /*player*/, Milliseconds /*currentTime*/) {}

void arduinoUiLoop(Player& player, const Network& wifiNetwork, const Network& bleNetwork, Milliseconds currentMillis) {
  updateButtons(currentMillis);  // Read, debounce, and process the buttons
#if !JL_BUTTONS_DISABLED
  uint8_t btn = buttonStatus(0, currentMillis);

#if ATOM_MATRIX_SCREEN
  if (atomScreenMessage(btn, currentMillis)) { return; }
#endif  // ATOM_MATRIX_SCREEN

#if JL_IS_CONFIG(FAIRY_WAND)
  atomScreenClear();
  atomScreenDisplay(currentMillis);
  if (BTN_EVENT(btn)) { player.triggerPatternOverride(currentMillis); }
  return;
#endif  // FAIRY_WAND

#if JL_BUTTON_LOCK
  // 0 Locked and awaiting click; 1 Awaiting long press; 2 Awaiting click; 3 Awaiting long press; 4 Awaiting release; 5
  // Unlocked
  static uint8_t buttonLockState = 0;
  static Milliseconds lockButtonTime = 0;

  // If idle-time expired, return to ‘locked’ state
  if (buttonLockState != 0 && currentMillis - lockButtonTime >= 0) { buttonLockState = 0; }

  if (buttonLockState < 5) {
    if (BTN_EVENT(btn)) {
      // If we don’t receive the correct button event for the state we’re currently in, return immediately to state 0.
      // In odd states (1,3) we want a long press; in even states (0,2) we want a brief press-and-release.
      if (btn != ((buttonLockState & 1) ? BTN_LONGPRESS : BTN_RELEASED)) {
        buttonLockState = 0;
      } else {
        buttonLockState++;
        // To reject accidental presses, exit unlock sequence if four seconds without progress
        lockButtonTime = currentMillis + 4000;
      }
    }

    // We show a blank display:
    // 1. When in fully locked mode, with the button not pressed
    // 2. When the button has been pressed long enough to register as a long press, and we want to signal the user to
    // let go now
    // 3. In the final transition from state 4 (awaiting release) to state 5 (unlocked)
    if ((buttonLockState == 0 && btn == BTN_IDLE) || BTN_SHORT_PRESS_COMPLETE(btn) || buttonLockState >= 4) {
      atomScreenClear();
    } else if (buttonLockState & 1) {
      // In odd  states (1,3) we show "L".
      atomScreenLong(player, wifiNetwork, bleNetwork, currentMillis);
    } else {
      // In even states (0,2) we show "S".
      atomScreenShort(player, wifiNetwork, bleNetwork, currentMillis);
    }
    atomScreenDisplay(currentMillis);

    // In lock state 4, wait for release of the button, and then move to state 5 (fully unlocked)
    if (buttonLockState < 4 || btn != BTN_IDLE) { return; }
    buttonLockState = 5;
    lockButtonTime = currentMillis + lockDelay;
  } else if (btn != BTN_IDLE) {
    lockButtonTime = currentMillis + lockDelay;
  }
#endif  // JL_BUTTON_LOCK

  switch (btn) {
    case BTN_RELEASED:
#if ATOM_MATRIX_SCREEN
      modeAct(player, currentMillis);
#endif  // ATOM_MATRIX_SCREEN
      break;

    case BTN_LONGPRESS:
#if ATOM_MATRIX_SCREEN
      nextMode(player, currentMillis);
#endif  // ATOM_MATRIX_SCREEN
      break;

    case BTN_LONGLONGPRESS:
#if ATOM_MATRIX_SCREEN
      menuMode = kSpecial;
#endif  // ATOM_MATRIX_SCREEN
      break;
  }
#if ATOM_MATRIX_SCREEN
  atomScreenUnlocked(player, wifiNetwork, bleNetwork, currentMillis);
  atomScreenDisplay(currentMillis);
#endif  // ATOM_MATRIX_SCREEN
#endif  // !JL_BUTTONS_DISABLED
}

uint8_t getBrightness() { return brightnessList[brightnessCursor]; }

}  // namespace jazzlights

#endif
