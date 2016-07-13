#ifndef DFSPARKS_LOG_H
#define DFSPARKS_LOG_H
#include <stdarg.h>

namespace dfsparks {
extern enum LogLevel { errorLevel, infoLevel, debugLevel, maxLevel } logLevel;

void log(LogLevel level, const char *fmt, va_list args);

inline void debug(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log(debugLevel, fmt, args);
  va_end(args);
}

inline void info(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log(infoLevel, fmt, args);
  va_end(args);
}

inline void error(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log(errorLevel, fmt, args);
  va_end(args);
}

} // namespace dfsparks
#endif /* DFSPARKS_LOG_H */
