#include "jazzlights/util/log.h"

#include <ctype.h>

namespace jazzlights {

namespace {

void EscapeRawBuffer(const BufferViewU8 input, BufferViewU8* output) {
  size_t out_idx = 0;
  for (size_t i = 0; i < input.size(); i++) {
    uint8_t c = input[i];
    if (c == '\\') {
      if (out_idx + 2 >= output->size()) { break; }
      (*output)[out_idx++] = '\\';
      (*output)[out_idx++] = '\\';
    } else if (isprint(c) || c == '\n' || c == '\t') {
      if (out_idx + 1 >= output->size()) { break; }
      (*output)[out_idx++] = static_cast<uint8_t>(c);
    } else if (c == 0) {
      if (out_idx + 2 >= output->size()) { break; }
      (*output)[out_idx++] = '\\';
      (*output)[out_idx++] = '0';
    } else {
      if (out_idx + 4 >= output->size()) { break; }
      int written = snprintf((char*)&(*output)[out_idx], 5, "\\x%02X", c);
      out_idx += static_cast<size_t>(written);
    }
  }
  (*output)[out_idx] = '\0';
  output->resize(out_idx);
}

}  // namespace

std::unique_lock<std::mutex> GetEscapeBufferLock() {
  static std::mutex sMutex;
  return std::unique_lock<std::mutex>(sMutex);
}

BufferViewU8 EscapeIntoStaticBuffer(const BufferViewU8 input) {
  static OwnedBufferU8 escapedBuffer(64);
  size_t maxEscapeLengthNeeded = input.size() * 4;
  if (escapedBuffer.size() < maxEscapeLengthNeeded) { escapedBuffer.resize(maxEscapeLengthNeeded); }
  BufferViewU8 output(escapedBuffer);
  EscapeRawBuffer(input, &output);
  return output;
}

void SetupLogging() {
#if JL_M5_LOGGING
  M5.Log.setLogLevel(m5::log_target_serial, is_debug_logging_enabled() ? ESP_LOG_DEBUG : ESP_LOG_INFO);
  M5.Log.setLogLevel(m5::log_target_display, ESP_LOG_NONE);
  M5.Log.setEnableColor(m5::log_target_serial, true);
#endif  // JL_M5_LOGGING
}

}  // namespace jazzlights