#ifndef JL_ARDUINO_LOOP_H
#define JL_ARDUINO_LOOP_H

#include "jazzlights/config.h"

#ifdef ARDUINO

namespace jazzlights {

void arduinoSetup(void);
void arduinoLoop(void);

}  // namespace jazzlights

#endif  // ARDUINO

#endif  // JL_ARDUINO_LOOP_H
