#include "jazzlights/util/log.h"

#include <ctype.h>

namespace jazzlights {

size_t EscapeRawBuffer(const uint8_t* input, size_t inputLength, uint8_t* output, size_t outputSize) {
  if (input == nullptr || output == nullptr || outputSize == 0) { return 0; }
  size_t out_idx = 0;
  for (size_t i = 0; i < inputLength; i++) {
    uint8_t c = input[i];
    if (c == '\\') {
      if (out_idx + 2 >= outputSize) { break; }
      output[out_idx++] = '\\';
      output[out_idx++] = '\\';
    } else if (isprint(c) || c == '\n' || c == '\t') {
      if (out_idx + 1 >= outputSize) { break; }
      output[out_idx++] = static_cast<uint8_t>(c);
    } else if (c == 0) {
      if (out_idx + 2 >= outputSize) { break; }
      output[out_idx++] = '\\';
      output[out_idx++] = '0';
    } else {
      if (out_idx + 4 >= outputSize) { break; }
      int written = snprintf((char*)&output[out_idx], 5, "\\x%02X", c);
      out_idx += static_cast<size_t>(written);
    }
  }
  output[out_idx] = '\0';
  return out_idx;
}

const std::unique_lock<std::mutex> GetEscapeBufferLock() {
  static std::mutex sMutex;
  return std::unique_lock<std::mutex>(sMutex);
}

uint8_t* EscapeIntoStaticBuffer(const uint8_t* input, size_t inputLength) {
  static uint8_t* escapedBuffer = nullptr;
  static size_t escapedBufferLength = 0;
  size_t maxEscapeLengthNeeded = inputLength * 4;
  if (escapedBufferLength < maxEscapeLengthNeeded) {
    escapedBufferLength = maxEscapeLengthNeeded;
    escapedBuffer = reinterpret_cast<uint8_t*>(realloc(escapedBuffer, escapedBufferLength));
    if (escapedBuffer == nullptr) { jll_fatal("Failed to realloc escapedBuffer %zu", escapedBufferLength); }
  }
  EscapeRawBuffer(input, inputLength, escapedBuffer, escapedBufferLength);
  return escapedBuffer;
}

}  // namespace jazzlights