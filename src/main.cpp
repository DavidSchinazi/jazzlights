#include <Jazzlights.h>

#ifdef JAZZLIGHTS_PLATFORMIO

void setup() { jazzlights::vestSetup(); }

void loop() { jazzlights::vestLoop(); }

#endif  // JAZZLIGHTS_PLATFORMIO
