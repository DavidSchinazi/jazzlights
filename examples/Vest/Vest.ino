#define WEARABLE 1

#include "JazzLights.h"

void setup() {
  jazzlights::vestSetup();
}

void loop() {
  jazzlights::vestLoop();
}
