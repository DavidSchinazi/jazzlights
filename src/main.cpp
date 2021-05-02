#include <Unisparks.h>

#ifdef UNISPARKS_PLATFORMIO

void setup() {
  unisparks::vestSetup();
}

void loop() {
  unisparks::vestLoop();
}

#endif // UNISPARKS_PLATFORMIO
