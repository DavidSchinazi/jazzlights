#ifndef JL_ARDUINO_LOOP_H
#define JL_ARDUINO_LOOP_H

#include "jazzlights/config.h"

#ifdef ESP32

namespace jazzlights {

void arduinoSetup(void);
void arduinoLoop(void);

}  // namespace jazzlights

#endif  // ESP32

#endif  // JL_ARDUINO_LOOP_H
