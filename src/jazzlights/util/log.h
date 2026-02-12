#ifndef JL_UTIL_LOG_H
#define JL_UTIL_LOG_H

#include <cstdio>
#include <cstdlib>

namespace jazzlights {

inline bool is_debug_logging_enabled() { return false; }

size_t EscapeRawBuffer(const uint8_t* input, size_t input_len, uint8_t* output, size_t out_size);

}  // namespace jazzlights

// On ESP32, printf automatically prints to UART 0, which is already properly configured to 115200 8-N-1.
// We considered replacing our custom logging with esp_log, however the Arduino Core for ESP-IDF hard-codes the max log
// level to error-only at compile time (see CONFIG_LOG_MAXIMUM_LEVEL=1). Additionally, it logs function, file, and line
// number - and that makes logs longer than a screen. We could revisit this if we end up compiling our own ESP-IDF.

#define _LOG_AT_LEVEL(levelStr, format, ...) ::printf(levelStr ": " format "\n", ##__VA_ARGS__)

#define jll_debug(format, ...)                                                         \
  do {                                                                                 \
    if (is_debug_logging_enabled()) { _LOG_AT_LEVEL("DEBUG", format, ##__VA_ARGS__); } \
  } while (0)

#define jll_info(format, ...) _LOG_AT_LEVEL(" INFO", format, ##__VA_ARGS__)
#define jll_error(format, ...) _LOG_AT_LEVEL("ERROR", format, ##__VA_ARGS__)

#define jll_fatal(format, ...)                     \
  do {                                             \
    _LOG_AT_LEVEL("FATAL", format, ##__VA_ARGS__); \
    abort();                                       \
  } while (0)

#endif  // JL_UTIL_LOG_H
