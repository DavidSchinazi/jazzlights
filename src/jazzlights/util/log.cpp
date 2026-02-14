#include "jazzlights/util/log.h"

#include <ctype.h>

namespace jazzlights {

size_t EscapeRawBuffer(const uint8_t* input, size_t input_len, uint8_t* output, size_t out_size) {
  if (input == nullptr || output == nullptr || out_size == 0) { return 0; }
  size_t out_idx = 0;
  for (size_t i = 0; i < input_len; i++) {
    uint8_t c = input[i];
    if (c == '\\') {
      if (out_idx + 2 >= out_size) { break; }
      output[out_idx++] = '\\';
      output[out_idx++] = '\\';
    } else if (isprint(c) || c == '\n' || c == '\t') {
      if (out_idx + 1 >= out_size) { break; }
      output[out_idx++] = static_cast<uint8_t>(c);
    } else if (c == 0) {
      if (out_idx + 2 >= out_size) { break; }
      output[out_idx++] = '\\';
      output[out_idx++] = '0';
    } else {
      if (out_idx + 4 >= out_size) { break; }
      int written = snprintf((char*)&output[out_idx], 5, "\\x%02X", c);
      out_idx += static_cast<size_t>(written);
    }
  }
  output[out_idx] = '\0';
  return out_idx;
}

}  // namespace jazzlights