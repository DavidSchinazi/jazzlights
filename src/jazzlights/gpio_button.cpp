#include "jazzlights/gpio_button.h"

#ifdef ARDUINO

#include <Arduino.h>

#include "jazzlights/util/time.h"

namespace jazzlights {

#define NUMBUTTONS 1
uint8_t buttonPins[NUMBUTTONS] = {39};

#define BTN_DEBOUNCETIME 20
#define BTN_LONGPRESSTIME 1000

Milliseconds buttonEvents[NUMBUTTONS];
uint8_t buttonStatuses[NUMBUTTONS];

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

void setupButtons(Milliseconds /*currentTime*/) {
  for (uint8_t i = 0; i < sizeof(buttonPins) / sizeof(buttonPins[0]); i++) { pinMode(buttonPins[i], INPUT_PULLUP); }
}

}  // namespace jazzlights

#endif  // ARDUINO
