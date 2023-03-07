#ifndef JL_LOG_H
#define JL_LOG_H

namespace jazzlights {

extern bool gDebugLoggingEnabled;

inline bool is_debug_logging_enabled() { return gDebugLoggingEnabled; }

inline void enable_debug_logging() { gDebugLoggingEnabled = true; }

#ifdef ARDUINO

__attribute__((format(printf, 1, 2))) void arduinoLog(const char* format, ...);

#define _LOG_AT_LEVEL(levelStr, format, ...) arduinoLog(levelStr ": " format, ##__VA_ARGS__)

#else  // ARDUINO

#define _LOG_AT_LEVEL(levelStr, format, ...) printf(levelStr ": " format "\n", ##__VA_ARGS__)

#endif  // ARDUINO

#define jll_debug(format, ...)                                                         \
  do {                                                                                 \
    if (is_debug_logging_enabled()) { _LOG_AT_LEVEL("DEBUG", format, ##__VA_ARGS__); } \
  } while (0)

#define jll_info(format, ...) _LOG_AT_LEVEL("INFO", format, ##__VA_ARGS__)
#define jll_error(format, ...) _LOG_AT_LEVEL("ERROR", format, ##__VA_ARGS__)

#define jll_fatal(format, ...)                     \
  do {                                             \
    _LOG_AT_LEVEL("FATAL", format, ##__VA_ARGS__); \
    abort();                                       \
  } while (0)

}  // namespace jazzlights

#endif  // JL_LOG_H
