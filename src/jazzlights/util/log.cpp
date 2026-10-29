#include "jazzlights/util/log.h"

#include <ctype.h>

#include <cstdarg>

namespace jazzlights {

namespace {

void EscapeRawBuffer(const BufferViewU8 input, BufferViewU8* output) {
  size_t outIdx = 0;
  for (size_t i = 0; i < input.size(); i++) {
    uint8_t c = input[i];
    if (c == '\\') {
      if (outIdx + 2 >= output->size()) { break; }
      (*output)[outIdx++] = '\\';
      (*output)[outIdx++] = '\\';
    } else if (isprint(c) || c == '\n' || c == '\t') {
      if (outIdx + 1 >= output->size()) { break; }
      (*output)[outIdx++] = static_cast<uint8_t>(c);
    } else if (c == 0) {
      if (outIdx + 2 >= output->size()) { break; }
      (*output)[outIdx++] = '\\';
      (*output)[outIdx++] = '0';
    } else {
      if (outIdx + 4 >= output->size()) { break; }
      int written = snprintf((char*)&(*output)[outIdx], 5, "\\x%02X", c);
      outIdx += static_cast<size_t>(written);
    }
  }
  (*output)[outIdx] = '\0';
  output->resize(outIdx);
}

}  // namespace

std::unique_lock<std::mutex> GetEscapeBufferLock() {
  static std::mutex sMutex;
  return std::unique_lock<std::mutex>(sMutex);
}

BufferViewU8 EscapeIntoStaticBuffer(const BufferViewU8 input) {
  static OwnedBufferU8 sEscapedBuffer(64);
  size_t maxEscapeLengthNeeded = input.size() * 4;
  if (sEscapedBuffer.size() < maxEscapeLengthNeeded) { sEscapedBuffer.resize(maxEscapeLengthNeeded); }
  BufferViewU8 output(sEscapedBuffer);
  EscapeRawBuffer(input, &output);
  return output;
}

void AppendPrintf(std::string* str, const char* format, ...) {
  va_list args;
  va_start(args, format);
  va_list argsCopy;
  va_copy(argsCopy, args);
  const int length = vsnprintf(nullptr, 0, format, argsCopy);
  va_end(argsCopy);
  if (length > 0) {
    const size_t oldSize = str->size();
    str->resize(oldSize + static_cast<size_t>(length));
    vsnprintf(&(*str)[oldSize], static_cast<size_t>(length) + 1, format, args);
  }
  va_end(args);
}

void SetupLogging() {
#if JL_M5_LOGGING
  M5.Log.setLogLevel(m5::log_target_serial, IsDebugLoggingEnabled() ? ESP_LOG_DEBUG : ESP_LOG_INFO);
  M5.Log.setLogLevel(m5::log_target_display, ESP_LOG_NONE);
  M5.Log.setEnableColor(m5::log_target_serial, true);
#endif  // JL_M5_LOGGING
}

}  // namespace jazzlights