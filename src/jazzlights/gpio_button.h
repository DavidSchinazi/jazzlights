#ifndef JL_GPIO_BUTTON_H
#define JL_GPIO_BUTTON_H

#include "jazzlights/config.h"

#ifdef ARDUINO

#include "jazzlights/util/time.h"

namespace jazzlights {

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

void setupButtons(Milliseconds currentTime);
void updateButtons(const Milliseconds currentMillis);
uint8_t buttonStatus(uint8_t buttonNum, const Milliseconds currentMillis);

}  // namespace jazzlights

#endif  // ARDUINO
#endif  // JL_GPIO_BUTTON_H
