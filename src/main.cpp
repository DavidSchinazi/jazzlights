#include "jazzlights/config.h"

#ifdef ARDUINO

#include <Arduino.h>

#include "jazzlights/vest.h"

void setup() { jazzlights::vestSetup(); }

void loop() { jazzlights::vestLoop(); }

#else  // ARDUINO

int main(int /*argc*/, char** /*argv*/) { return 0; }

#endif  // ARDUINO
