#include "unisparks/util/log.hpp"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

namespace unisparks {

const char* level_str[] = {"FATAL", "ERROR", "WARNING", "INFO", "DEBUG"};
LogLevel logLevel = LogLevel::INFO;

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


} // namespace unisparks

