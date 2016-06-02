#include "DFSparks_Log.h"
#include <stdio.h>
#include <stdarg.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace dfsparks {

  void log(const char *level, const char *fmt, va_list args) {
#ifdef ARDUINO
    char buf[128];
    Serial.print(level);
    Serial.print(": ");
    vsnprintf(buf, 128, fmt, args);
    Serial.println(buf);
#else
    printf("%s: ", level);
    vprintf(fmt, args);
    printf("\n");
#endif
  }

} // namespace dfsparks
