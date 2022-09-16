#include "jazzlights/util/log.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

namespace jazzlights {

const char* level_str[] = {"FATAL", "ERROR", "WARNING", "INFO", "DEBUG"};
#if 0
LogLevel logLevel = LogLevel::DEBUG;
#else
LogLevel logLevel = LogLevel::INFO;
#endif

void defaultHandler(const LogMessage& msg);
LogHandler logHandler = defaultHandler;

void setLogHandler(LogHandler fn) {
  logHandler = fn;
}

void log(LogLevel level, const char* fmt, va_list args) {
  if (level > logLevel) {
    return;
  }
  char buf[256];
  vsnprintf(buf, sizeof(buf), fmt, args);
  LogMessage msg {level, buf};
  logHandler(msg);  
  if (level == LogLevel::FATAL) {
    abort();
  }
}


}  // namespace jazzlights

