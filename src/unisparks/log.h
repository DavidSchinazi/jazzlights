#ifndef UNISPARKS_LOG_H
#define UNISPARKS_LOG_H
#include <stdarg.h>

#if __cplusplus >= 201402L
#define UNISPARKS_DEPRECATED_MSG(msg) [[deprecated(msg)]]
#define UNISPARKS_DEPRECATED_S
#else
#define UNISPARKS_DEPRECATED(msg) 
#define UNISPARKS_DEPRECATED_S __attribute__((deprecated))
#endif

namespace unisparks {
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

} // namespace unisparks
#endif /* UNISPARKS_LOG_H */
