#ifndef JL_SHIM_FREERTOS_H
#define JL_SHIM_FREERTOS_H

#include <cstdint>

using BaseType_t = int;
using UBaseType_t = unsigned int;
using TickType_t = uint32_t;
using TaskHandle_t = void*;

inline constexpr BaseType_t pdPASS = 1;
inline constexpr UBaseType_t tskIDLE_PRIORITY = 0;
#define pdMS_TO_TICKS(ms) (static_cast<TickType_t>(ms))

#endif  // JL_SHIM_FREERTOS_H
