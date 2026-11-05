#ifndef JL_SHIM_ESP_RANDOM_H
#define JL_SHIM_ESP_RANDOM_H

#include <stdlib.h>
#include <sys/random.h>
#include <unistd.h>

#include <cstddef>

inline void esp_fill_random(void* buffer, size_t length) {
  // getentropy can only provide 256 bytes at a time.
  char* output = static_cast<char*>(buffer);
  while (length > 0) {
    const size_t chunk = length < 256 ? length : 256;
    if (getentropy(output, chunk) != 0) { abort(); }
    output += chunk;
    length -= chunk;
  }
}

#endif  // JL_SHIM_ESP_RANDOM_H
