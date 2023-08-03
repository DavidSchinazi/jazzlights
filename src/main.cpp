#include "jazzlights/config.h"

#ifdef JAZZLIGHTS_PLATFORMIO

#include <Arduino.h>

#include "jazzlights/vest.h"

void setup() { jazzlights::vestSetup(); }

void loop() { jazzlights::vestLoop(); }

#else  // JAZZLIGHTS_PLATFORMIO

int main(int /*argc*/, char** /*argv*/) { return 0; }

#endif  // JAZZLIGHTS_PLATFORMIO
