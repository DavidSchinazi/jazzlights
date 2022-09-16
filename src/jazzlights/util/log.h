#ifndef JAZZLIGHTS_LOG_H
#define JAZZLIGHTS_LOG_H
#include <stdarg.h>
#include <stdlib.h>

#if __cplusplus >= 201402L
#define JAZZLIGHTS_DEPRECATED(msg, code) [[deprecated(msg)]] code
#else
#define JAZZLIGHTS_DEPRECATED(msg) code __attribute__((deprecated))
#endif

namespace jazzlights {
enum class LogLevel { FATAL, ERROR, WARNING, INFO, DEBUG, MAX };
extern LogLevel logLevel;

inline bool is_debug_logging_enabled() {
  return logLevel >= LogLevel::DEBUG;
}

void log(LogLevel level, const char* fmt, va_list args);

inline void debug(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log(LogLevel::DEBUG, fmt, args);
  va_end(args);
}

inline void info(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log(LogLevel::INFO, fmt, args);
  va_end(args);
}

inline void warn(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log(LogLevel::WARNING, fmt, args);
  va_end(args);
}

inline void error(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log(LogLevel::ERROR, fmt, args);
  va_end(args);
}

[[noreturn]]
inline void fatal(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log(LogLevel::FATAL, fmt, args);
  va_end(args);
  abort(); // this is redundant
}

inline void enableVerboseOutput() {
  logLevel = LogLevel::DEBUG;
}

struct LogMessage {
  LogLevel level;
  const char* text;
};

using LogHandler = void (*)(const LogMessage& msg);

void setLogHandler(LogHandler handler);

}  // namespace jazzlights
#endif  // JAZZLIGHTS_LOG_H
