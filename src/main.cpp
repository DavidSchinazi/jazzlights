#include "jazzlights/config.h"

#ifdef JAZZLIGHTS_PLATFORMIO

#include <Arduino.h>

#include "jazzlights/device.h"

void setup() { jazzlights::deviceSetup(); }

void loop() { jazzlights::deviceLoop(); }

#endif  // JAZZLIGHTS_PLATFORMIO
