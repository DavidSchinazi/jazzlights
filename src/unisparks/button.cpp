#include <Unisparks.h>

#if WEARABLE

#include <Arduino.h>
#include <cstdint>

namespace unisparks {

#ifndef BUTTONS_DISABLED
#  define BUTTONS_DISABLED 0
#endif // BUTTONS_DISABLED

#ifndef START_DIM
#  define START_DIM 0
#endif // START_DIM

#ifndef ATOM_MATRIX_SCREEN
#  ifdef ESP32
#    define ATOM_MATRIX_SCREEN 1
#  else // ESP32
#    define ATOM_MATRIX_SCREEN 0
#  endif // ESP32
#endif // ATOM_MATRIX_SCREEN

#ifdef ATOM_MATRIX_SCREEN
#define ATOM_SCREEN_NUM_LEDS 25
CRGB atomScreenLEDs[ATOM_SCREEN_NUM_LEDS] = {};
#endif // ATOM_MATRIX_SCREEN

#if defined(ESP32)
#define NUMBUTTONS 1
uint8_t buttonPins[NUMBUTTONS] = {39};
#elif defined(ESP8266)
// D0 = GPIO 16
// D1 = GPIO  5
// D2 = GPIO  4
// D3 = GPIO  0 = bottom-right button
// D4 = GPIO  2
// 3.3V
// GND
// D5 = GPIO 14 = top-right button
// D6 = GPIO 12 = bottom-left button
// D7 = GPIO 13 = top-left button
// D8 = GPIO 15
// D9 = GPIO  3
//D10 = GPIO 1
// GND
// 3.3V

// Button settings
#define NUMBUTTONS 4
#define MODEBUTTON 13
#define BRIGHTNESSBUTTON 14
#define WIFIBUTTON 12
#define SPECIALBUTTON 0
uint8_t buttonPins[NUMBUTTONS] = {MODEBUTTON, BRIGHTNESSBUTTON, WIFIBUTTON, SPECIALBUTTON};
#endif // ESPxx

#define BTN_IDLE 0
#define BTN_DEBOUNCING 1
#define BTN_PRESSED 2
#define BTN_RELEASED 3
#define BTN_LONGPRESS 4
#define BTN_LONGPRESSREAD 5

#define BTN_DEBOUNCETIME 20
#define BTN_LONGPRESSTIME 1000

unsigned long buttonEvents[NUMBUTTONS];
uint8_t buttonStatuses[NUMBUTTONS];

static constexpr uint32_t lockDelay = 10000;

const uint8_t brightnessList[] = { 2, 4, 8, 16, 32, 64, 128, 255 };
#define NUM_BRIGHTNESSES (sizeof(brightnessList) / sizeof(brightnessList[0]))
#define MAX_BRIGHTNESS (NUM_BRIGHTNESSES-1)

#ifndef FIRST_BRIGHTNESS
#define FIRST_BRIGHTNESS 3
#endif // FIRST_BRIGHTNESS

uint8_t brightnessCursor = FIRST_BRIGHTNESS;

#if ATOM_MATRIX_SCREEN

enum atomMenuMode {
    kNext,
    kPrevious,
    kBrightness,
    kSpecial,
};

atomMenuMode menuMode = kNext;

void nextMode(Player& /*player*/, uint32_t /*currentMillis*/) {
  switch (menuMode) {
    case kNext: menuMode = kPrevious; break;
    case kPrevious: menuMode = kBrightness; break;
    case kBrightness: menuMode = kSpecial; break;
    case kSpecial: menuMode = kNext; break;
  };
}

void modeAct(Player& player, uint32_t currentMillis) {
  switch (menuMode) {
    case kNext:
      info("%u Next button has been hit", currentMillis);
      player.stopSpecial();
      player.next();
      break;
    case kPrevious:
      info("%u Back button has been hit", currentMillis);
      player.stopSpecial();
      player.prev();
      player.loopOne();
      break;
    case kBrightness:
      brightnessCursor = (brightnessCursor + 1 < NUM_BRIGHTNESSES) ? brightnessCursor + 1 : 0;
      info("%u Brightness button has been hit %u", currentMillis, brightnessList[brightnessCursor]);
      break;
    case kSpecial:
      info("%u Special button has been hit", currentMillis);
      player.handleSpecial();
      break;
  };

}

static const CRGB nextColor = CRGB::Blue;
static const CRGB menuIconNext[ATOM_SCREEN_NUM_LEDS] = {
    nextColor, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black,
    nextColor, nextColor,   CRGB::Black, CRGB::Black, CRGB::Black,
    nextColor, nextColor,   nextColor,   CRGB::Black, CRGB::Black,
    nextColor, nextColor,   CRGB::Black, CRGB::Black, CRGB::Black,
    nextColor, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black,
};

static const CRGB prevColor = CRGB::Red;
static const CRGB menuIconPrevious[ATOM_SCREEN_NUM_LEDS] = {
    CRGB::Black, CRGB::Black, prevColor, CRGB::Black, CRGB::Black,
    CRGB::Black, prevColor,   prevColor, CRGB::Black, CRGB::Black,
    prevColor,   prevColor,   prevColor, CRGB::Black, CRGB::Black,
    CRGB::Black, prevColor,   prevColor, CRGB::Black, CRGB::Black,
    CRGB::Black, CRGB::Black, prevColor, CRGB::Black, CRGB::Black,
};

static const CRGB menuIconBrightness[ATOM_SCREEN_NUM_LEDS] = {
    CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black,
    CRGB::DarkGoldenrod, CRGB::Goldenrod, CRGB::DarkGoldenrod, CRGB::Black, CRGB::Black,
    CRGB::Goldenrod, CRGB::LightGoldenrodYellow, CRGB::Goldenrod, CRGB::Black, CRGB::Black,
    CRGB::DarkGoldenrod, CRGB::Goldenrod, CRGB::DarkGoldenrod, CRGB::Black, CRGB::Black,
    CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black,
};

static const CRGB specColor = CRGB::Green;
static const CRGB menuIconSpecial[ATOM_SCREEN_NUM_LEDS] = {
    specColor,   specColor,   specColor,   CRGB::Black, CRGB::Black,
    CRGB::Black, CRGB::Black, specColor,   CRGB::Black, CRGB::Black,
    CRGB::Black, specColor,   CRGB::Black, CRGB::Black, CRGB::Black,
    CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black,
    CRGB::Black, specColor,   CRGB::Black, CRGB::Black, CRGB::Black,
};

CLEDController* atomMatrixScreenController = nullptr;

void atomScreenDisplay() {
  // M5Stack recommends not setting the atom screen brightness greater
  // than 20 to avoid melting the screen/cover over the LEDs.
  atomMatrixScreenController->showLeds(20);
}

void atomScreenNetwork(Player& player, uint32_t /*currentMillis*/) {
  // Change top-right Atom matrix screen LED based on network status.
  CRGB networkColor = CRGB::Blue;
  Network* network = player.network();
  if (network != nullptr) {
    switch (player.network()->status()) {
      case INITIALIZING: networkColor = CRGB::Pink; break;
      case DISCONNECTED: networkColor = CRGB::Purple; break;
      case CONNECTING: networkColor = CRGB::Yellow; break;
      case CONNECTED: networkColor = CRGB::Green; break;
      case DISCONNECTING: networkColor = CRGB::Orange; break;
      case CONNECTION_FAILED: networkColor = CRGB::Red; break;
    }
  }
  atomScreenLEDs[4] = networkColor;
}

void atomScreenUnlocked(Player& player, uint32_t currentMillis) {
  const CRGB* icon = atomScreenLEDs;
  switch (menuMode) {
    case kNext: icon = menuIconNext; break;
    case kPrevious: icon = menuIconPrevious; break;
    case kBrightness: icon = menuIconBrightness; break;
    case kSpecial: icon = menuIconSpecial; break;
  };
  for (int i = 0; i < ATOM_SCREEN_NUM_LEDS; i++) {
    atomScreenLEDs[i] = icon[i];
  }
  atomScreenNetwork(player, currentMillis);
}

void atomScreenClear() {
  for (int i = 0; i < ATOM_SCREEN_NUM_LEDS; i++) {
    atomScreenLEDs[i] = CRGB::Black;
  }
}

void atomScreenLong(Player& player, uint32_t currentMillis) {
  atomScreenClear();
  for (int i : {0,5,10,15,20,21,22}) {
    atomScreenLEDs[i] = CRGB::Gold;
  }
  atomScreenNetwork(player, currentMillis);
}

void atomScreenShort(Player& player, uint32_t currentMillis) {
  atomScreenClear();
  for (int i : {2,1,0,5,10,11,12,17,22,21,20}) {
    atomScreenLEDs[i] = CRGB::Gold;
  }
  atomScreenNetwork(player, currentMillis);
}

#endif // ATOM_MATRIX_SCREEN


void updateButtons(uint32_t currentMillis) {
  for (int i = 0; i < NUMBUTTONS; i++) {
    switch (buttonStatuses[i]) {
      case BTN_IDLE:
        if (digitalRead(buttonPins[i]) == LOW) {
          buttonEvents[i] = currentMillis;
          buttonStatuses[i] = BTN_DEBOUNCING;
        }
        break;

      case BTN_DEBOUNCING:
        if (currentMillis - buttonEvents[i] > BTN_DEBOUNCETIME) {
          if (digitalRead(buttonPins[i]) == LOW) {
            buttonStatuses[i] = BTN_PRESSED;
          }
        }
        break;

      case BTN_PRESSED:
        if (digitalRead(buttonPins[i]) == HIGH) {
          buttonStatuses[i] = BTN_RELEASED;
        } else if (currentMillis - buttonEvents[i] > BTN_LONGPRESSTIME) {
          buttonStatuses[i] = BTN_LONGPRESS;
        }
        break;

      case BTN_RELEASED:
        break;

      case BTN_LONGPRESS:
        break;

      case BTN_LONGPRESSREAD:
        if (digitalRead(buttonPins[i]) == HIGH) {
          buttonStatuses[i] = BTN_IDLE;
        }
        break;
    }
  }
}

uint8_t buttonStatus(uint8_t buttonNum) {
  uint8_t tempStatus = buttonStatuses[buttonNum];
  if (tempStatus == BTN_RELEASED) {
    buttonStatuses[buttonNum] = BTN_IDLE;
  } else if (tempStatus == BTN_LONGPRESS) {
    buttonStatuses[buttonNum] = BTN_LONGPRESSREAD;
  }
  return tempStatus;
}

void setupButtons() {
  for (uint8_t i = 0 ; i < sizeof(buttonPins) / sizeof(buttonPins[0]) ; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
#if ATOM_MATRIX_SCREEN
  atomMatrixScreenController = &FastLED.addLeds<WS2812, /*DATA_PIN=*/27, GRB>(atomScreenLEDs,
                                                          ATOM_SCREEN_NUM_LEDS);
  atomScreenClear();
#endif // ATOM_MATRIX_SCREEN
}

void doButtons(Player& player, uint32_t currentMillis) {
#if !BUTTONS_DISABLED
#if defined(ESP32)
  uint8_t btn = buttonStatus(0);
#if BUTTON_LOCK
  // info("doButtons start");
  static uint8_t buttonLockState = 0;
  static uint32_t lastButtonTime = 0;

  if (buttonLockState != 0 && currentMillis > lastButtonTime + lockDelay) {
    buttonLockState = 0;
  }
  if (btn == BTN_RELEASED ||  btn == BTN_LONGPRESS) {
    lastButtonTime = currentMillis;

    if (buttonLockState == 0) {
      buttonLockState++;
    } else if (buttonLockState == 1) {
      if (btn == BTN_LONGPRESS) {
        buttonLockState++;
      } else {
        buttonLockState = 0;
      }
    } else if (buttonLockState == 2) {
      if (btn == BTN_RELEASED) {
        buttonLockState++;
      } else {
        buttonLockState = 0;
      }
    } else if (buttonLockState == 3) {
      if (btn == BTN_LONGPRESS) {
        buttonLockState++;
      } else {
        buttonLockState = 0;
      }
    }
  }

  if (buttonLockState == 0) {
    atomScreenClear();
  } else if ((buttonLockState % 2) == 1) {
    atomScreenLong(player, currentMillis);
  } else if (buttonLockState == 2) {
    atomScreenShort(player, currentMillis);
  }

  if (buttonLockState < 4) {
    atomScreenDisplay();
    return;
  } else if (buttonLockState == 4) {
    buttonLockState = 5;
    btn = BTN_IDLE;
  }
#endif // BUTTON_LOCK
#if ATOM_MATRIX_SCREEN
#endif // ATOM_MATRIX_SCREEN
  switch (btn) {
    case BTN_RELEASED:
#if ATOM_MATRIX_SCREEN
    modeAct(player, currentMillis);
#endif // ATOM_MATRIX_SCREEN
      break;

    case BTN_LONGPRESS:
#if ATOM_MATRIX_SCREEN
    nextMode(player, currentMillis);
#endif // ATOM_MATRIX_SCREEN
      break;
  }
#if ATOM_MATRIX_SCREEN
  atomScreenUnlocked(player, currentMillis);
  atomScreenDisplay();
#endif // ATOM_MATRIX_SCREEN
#elif defined(ESP8266)
  const uint8_t btn0 = buttonStatus(0);
  const uint8_t btn1 = buttonStatus(1);
  const uint8_t btn2 = buttonStatus(2);
  const uint8_t btn3 = buttonStatus(3);

#if BUTTON_LOCK
  static uint8_t buttonLockState = 0;
  static uint32_t lastUnlockTime = 0;

  if (buttonLockState == 4 && currentMillis > lastUnlockTime + lockDelay) {
    buttonLockState = 0;
  }
  
  if (buttonLockState == 0 && btn0 == BTN_RELEASED) {
    buttonLockState++;
    return;
  }
  if (buttonLockState == 1 && btn3 == BTN_RELEASED) {
    buttonLockState++;
    return;
  }
  if (buttonLockState == 2 && btn2 == BTN_RELEASED) {
    buttonLockState++;
    return;
  }
  if (buttonLockState == 3 && btn1 == BTN_RELEASED) {
    buttonLockState++;
    lastUnlockTime = currentMillis;
    return;
  }

  if (buttonLockState < 4) {
    return;
  }

  lastUnlockTime = currentMillis;
#endif // BUTTON_LOCK
 
  // Check the mode button (for switching between effects)
  switch (btn0) {
    case BTN_RELEASED:
      info("Next button has been hit");
      player.stopSpecial();
      player.next();
      break;

    case BTN_LONGPRESS:
      info("Ignoring next button long press");
      break;
  }

  // Check the brightness adjust button
  switch (btn1) {
    case BTN_RELEASED:
      brightnessCursor = (brightnessCursor + 1 < NUM_BRIGHTNESSES) ? brightnessCursor + 1 : 0;
      info("Brightness button has been hit %u", brightnessList[brightnessCursor]);
      break;

    case BTN_LONGPRESS: // button was held down for a while
      brightnessCursor = FIRST_BRIGHTNESS;
      info("Brightness button long press %u", brightnessList[brightnessCursor]);
      break;
  }

  // Check the back button
  switch (btn2) {
    case BTN_RELEASED:
      info("Back button has been hit");
      player.stopSpecial();
      player.prev();
      player.loopOne();
      break;

    case BTN_LONGPRESS:
      info("Ignoring back button long press");
      break;
  }

  // Check the special button
  switch (btn3) {
    case BTN_RELEASED:
      info("Special button has been hit");
      player.handleSpecial();
      break;

    case BTN_LONGPRESS:
      info("Ignoring special button long press");
      break;
  }
#endif // ESPxx
#endif // !BUTTONS_DISABLED
}

uint8_t getBrightness() {
  return brightnessList[brightnessCursor];
}

} // namespace unisparks

#endif // WEARABLE
