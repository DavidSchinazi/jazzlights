#include "jazzlights/config.h"

#ifndef PIO_UNIT_TESTING

#ifdef ARDUINO

#include "jazzlights/arduino_loop.h"

void setup() { jazzlights::arduinoSetup(); }

void loop() { jazzlights::arduinoLoop(); }

#else  // ARDUINO

int main(int /*argc*/, char** /*argv*/) { return 0; }

#endif  // ARDUINO

#endif  // PIO_UNIT_TESTING
