#include "jazzlights/config.h"

#ifdef JL_PLATFORMIO

#include <Arduino.h>

#include "jazzlights/device.h"

void setup() { jazzlights::deviceSetup(); }

void loop() { jazzlights::deviceLoop(); }

#endif  // JL_PLATFORMIO
