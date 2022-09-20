#include "jazzlights/util/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif  // ARDUINO

namespace jazzlights {

bool gDebugLoggingEnabled = false;

#ifdef ARDUINO

void arduinoLog(const char* format, ...) {
  char buf[256];
  va_list args;
  va_start(args, format);
  const int numPrinted = vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  if (numPrinted >= sizeof(buf)) { buf[sizeof(buf) - 1] = '\0'; }
  Serial.println(buf);
}

#endif  // ARDUINO

}  // namespace jazzlights
