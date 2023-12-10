#include "jazzlights/config.h"

#ifdef ARDUINO

#include "jazzlights/arduino_loop.h"

void setup() { jazzlights::arduinoSetup(); }

void loop() { jazzlights::arduinoLoop(); }

#else  // ARDUINO

int main(int /*argc*/, char** /*argv*/) { return 0; }

#endif  // ARDUINO
