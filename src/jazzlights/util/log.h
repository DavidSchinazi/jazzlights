#ifndef JL_UTIL_LOG_H
#define JL_UTIL_LOG_H

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>

#include "jazzlights/util/config.h"

#if JL_M5_LOGGING

#if defined(INADDR_NONE) && ARDUINO
// When M5Unified is built with the Arduino core, it includes <Arduino.h> deep inside M5GFX. Unfortunately, that
// includes Arduino's <IPAddress.h> which uses INADDR_NONE as a global variable name. If we've previously included
// <lwip/inet.h>, that defined INADDR_NONE as a preprocessor macro so the two conflict. One potential solution is to
// include <Arduino.h> before <lwip/inet.h>, but it's simpler to just undefine it before including <M5Unified.h>.
#undef INADDR_NONE
#endif  // INADDR_NONE

#include <M5Unified.h>
#endif  // JL_M5_LOGGING

#include "jazzlights/util/buffer.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

inline bool IsDebugLoggingEnabled() { return false; }

// `EscapeIntoStaticBuffer()` uses a static malloc'ed buffer for the escaped string, so it is not thread-safe by
// default. Callers need to take a mutex using `GetEscapeBufferLock()`. Use the `jll_buffer*` macros below.
BufferViewU8 EscapeIntoStaticBuffer(const BufferViewU8 input);
std::unique_lock<std::mutex> GetEscapeBufferLock();

void SetupLogging();

}  // namespace jazzlights

// On ESP32, printf automatically prints to UART 0, which is already properly configured to 115200 8-N-1.
// We considered replacing our custom logging with esp_log, however the Arduino Core for ESP-IDF hard-codes the max log
// level to error-only at compile time (see CONFIG_LOG_MAXIMUM_LEVEL=1). Additionally, it logs function, file, and line
// number - and that makes logs longer than a screen. We could revisit this if we end up compiling our own ESP-IDF.

#define _JL_LOG_LEVEL_STRING_DEBUG "D"
#define _JL_LOG_LEVEL_STRING_INFO "I"
#define _JL_LOG_LEVEL_STRING_ERROR "E"
#define _JL_LOG_LEVEL_STRING_FATAL "F"

#define _LOG_AT_LEVEL(levelStr, format, ...) \
  ::printf("[%6lld][" levelStr "] " format "\n", MsSinceBootForLogs(), ##__VA_ARGS__)

#define _LOG_BUFFER_AT_LEVEL(levelStr, buffer, format, ...)                            \
  do {                                                                                 \
    std::unique_lock<std::mutex> lock(GetEscapeBufferLock());                          \
    _LOG_AT_LEVEL(levelStr, format " [%zu bytes]: %s", ##__VA_ARGS__, (buffer).size(), \
                  &EscapeIntoStaticBuffer(buffer)[0]);                                 \
  } while (0)

#if JL_M5_LOGGING

#define jll_debug(format, ...) M5_LOGD(format, ##__VA_ARGS__)
#define jll_info(format, ...) M5_LOGI(format, ##__VA_ARGS__)
#define jll_error(format, ...) M5_LOGE(format, ##__VA_ARGS__)

#define jll_fatal(format, ...)      \
  do {                              \
    M5_LOGE(format, ##__VA_ARGS__); \
    abort();                        \
  } while (0)

#else  // JL_M5_LOGGING

#define jll_debug(format, ...)                                                                         \
  do {                                                                                                 \
    if (IsDebugLoggingEnabled()) { _LOG_AT_LEVEL(_JL_LOG_LEVEL_STRING_DEBUG, format, ##__VA_ARGS__); } \
  } while (0)

#define jll_info(format, ...) _LOG_AT_LEVEL(_JL_LOG_LEVEL_STRING_INFO, format, ##__VA_ARGS__)
#define jll_error(format, ...) _LOG_AT_LEVEL(_JL_LOG_LEVEL_STRING_ERROR, format, ##__VA_ARGS__)

#define jll_fatal(format, ...)                                        \
  do {                                                                \
    _LOG_AT_LEVEL(_JL_LOG_LEVEL_STRING_FATAL, format, ##__VA_ARGS__); \
    abort();                                                          \
  } while (0)

#endif  // JL_M5_LOGGING

// Note that the jll_buffer_* variants use a static buffer and are therefore not thread safe.
#define jll_buffer_debug(buffer, format, ...)                                                                        \
  do {                                                                                                               \
    if (IsDebugLoggingEnabled()) { _LOG_BUFFER_AT_LEVEL(_JL_LOG_LEVEL_STRING_INFO, buffer, format, ##__VA_ARGS__); } \
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

#ifndef JL_SILENCE_PROTOCOL_LOGS
#define JL_SILENCE_PROTOCOL_LOGS 0
#endif  // JL_SILENCE_PROTOCOL_LOGS

#if JL_SILENCE_PROTOCOL_LOGS
#define jll_protocol_info(...) jll_debug(__VA_ARGS__)
#else  // JL_SILENCE_PROTOCOL_LOGS
#define jll_protocol_info(...) jll_info(__VA_ARGS__)
#endif  // JL_SILENCE_PROTOCOL_LOGS

#endif  // JL_UTIL_LOG_H
