#ifndef JL_SHIM_ESP_HEAP_CAPS_H
#define JL_SHIM_ESP_HEAP_CAPS_H

#include <cstddef>
#include <cstdint>

// The test server doesn't track the heap. main.cpp reports a fixed amount, which tests can choose.
inline constexpr uint32_t MALLOC_CAP_DEFAULT = 1 << 12;
size_t heap_caps_get_free_size(uint32_t caps);
size_t heap_caps_get_largest_free_block(uint32_t caps);

#endif  // JL_SHIM_ESP_HEAP_CAPS_H
