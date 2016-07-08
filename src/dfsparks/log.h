#ifndef DFSPARKS_LOG_H
#define DFSPARKS_LOG_H
#include <stdarg.h>

namespace dfsparks {
extern enum class LogLevel { error, info, debug, max } log_level;

void log(LogLevel level, const char *fmt, va_list args);

inline void debug(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log(LogLevel::debug, fmt, args);
  va_end(args);
}

inline void info(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log(LogLevel::info, fmt, args);
  va_end(args);
}

inline void error(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log(LogLevel::error, fmt, args);
  va_end(args);
}

} // namespace dfsparks
#endif /* DFSPARKS_LOG_H */
