#ifndef JL_UTIL_LOG_H
#define JL_UTIL_LOG_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>

#include "jazzlights/util/buffer.h"

namespace jazzlights {

inline bool is_debug_logging_enabled() { return false; }

size_t EscapeRawBuffer(const uint8_t* input, size_t inputLength, uint8_t* output, size_t outSize);

// `EscapeIntoStaticBuffer()` uses a static malloc'ed buffer for the escaped string, so it is not thread-safe by
// default. Callers need to take a mutex using `GetEscapeBufferLock()`. Use the `jll_buffer*` macros below.
uint8_t* EscapeIntoStaticBuffer(const uint8_t* input, size_t inputLength);
std::unique_lock<std::mutex> GetEscapeBufferLock();

}  // namespace jazzlights

// On ESP32, printf automatically prints to UART 0, which is already properly configured to 115200 8-N-1.
// We considered replacing our custom logging with esp_log, however the Arduino Core for ESP-IDF hard-codes the max log
// level to error-only at compile time (see CONFIG_LOG_MAXIMUM_LEVEL=1). Additionally, it logs function, file, and line
// number - and that makes logs longer than a screen. We could revisit this if we end up compiling our own ESP-IDF.

#define _JL_LOG_LEVEL_STRING_DEBUG "DEBUG"
#define _JL_LOG_LEVEL_STRING_INFO " INFO"
#define _JL_LOG_LEVEL_STRING_ERROR "ERROR"
#define _JL_LOG_LEVEL_STRING_FATAL "FATAL"

#define _LOG_AT_LEVEL(levelStr, format, ...) ::printf(levelStr ": " format "\n", ##__VA_ARGS__)

#define _LOG_BUFFER_AT_LEVEL(levelStr, buffer, format, ...)                            \
  do {                                                                                 \
    std::unique_lock<std::mutex> lock(GetEscapeBufferLock());                          \
    _LOG_AT_LEVEL(levelStr, format " [%zu bytes]: %s", ##__VA_ARGS__, (buffer).size(), \
                  EscapeIntoStaticBuffer(&(buffer)[0], (buffer).size()));              \
  } while (0)

#define jll_debug(format, ...)                                                                            \
  do {                                                                                                    \
    if (is_debug_logging_enabled()) { _LOG_AT_LEVEL(_JL_LOG_LEVEL_STRING_DEBUG, format, ##__VA_ARGS__); } \
  } while (0)

#define jll_info(format, ...) _LOG_AT_LEVEL(_JL_LOG_LEVEL_STRING_INFO, format, ##__VA_ARGS__)
#define jll_error(format, ...) _LOG_AT_LEVEL(_JL_LOG_LEVEL_STRING_ERROR, format, ##__VA_ARGS__)

#define jll_fatal(format, ...)                                        \
  do {                                                                \
    _LOG_AT_LEVEL(_JL_LOG_LEVEL_STRING_FATAL, format, ##__VA_ARGS__); \
    abort();                                                          \
  } while (0)

// Note that the jll_buffer_* variants use a static buffer and are therefore not thread safe.
#define jll_buffer_debug(buffer, format, ...)                                         \
  do {                                                                                \
    if (is_debug_logging_enabled()) {                                                 \
      _LOG_BUFFER_AT_LEVEL(_JL_LOG_LEVEL_STRING_INFO, buffer, format, ##__VA_ARGS__); \
    }                                                                                 \
  } while (0)
#define jll_buffer_info(buffer, format, ...) \
  _LOG_BUFFER_AT_LEVEL(_JL_LOG_LEVEL_STRING_INFO, buffer, format, ##__VA_ARGS__)
#define jll_buffer_error(buffer, format, ...) \
  _LOG_BUFFER_AT_LEVEL(_JL_LOG_LEVEL_STRING_ERROR, buffer, format, ##__VA_ARGS__)
#define jll_buffer_fatal(buffer, format, ...)                                        \
  do {                                                                               \
    _LOG_BUFFER_AT_LEVEL(_JL_LOG_LEVEL_STRING_FATAL, buffer, format, ##__VA_ARGS__); \
    abort();                                                                         \
  } while (0)

#define jll_buffer_debug2(bufferClass, ...) jll_buffer_debug(&(bufferClass)[0], (bufferClass).size(), ##__VA_ARGS__)

#endif  // JL_UTIL_LOG_H
