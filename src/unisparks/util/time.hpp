#ifndef UNISPARKS_TIME_H
#define UNISPARKS_TIME_H
#include <stdint.h>

namespace unisparks {

typedef int32_t Milliseconds;

constexpr Milliseconds ONE_SECOND = 1000;
constexpr Milliseconds ONE_MINUTE = 60 * ONE_SECOND;
constexpr Milliseconds MAX_TIME =  2147483647;

/**
 * Get monotonically increasing time in milliseconds
 */
Milliseconds timeMillis();

typedef int32_t FramesPerSecond;


} // namespace unisparks

#endif /* UNISPARKS_TIME_H */
