#ifndef JL_UTIL_TIME_H
#define JL_UTIL_TIME_H

#include "jazzlights/types.h"

namespace jazzlights {

using Microseconds = int64_t;

// Get monotonically increasing time in microseconds.
Microseconds timeMicros();

long long MillisecondsSinceBootForLogging();

using FramesPerSecond = int32_t;

inline constexpr Microseconds kMicrosecondsPerMillisecond = 1000;
inline constexpr Microseconds kMicrosecondsPerSecond = 1000000;

// Convert time from milliseconds to microseconds.
constexpr Microseconds MillisecondsToMicroseconds(int64_t timeMillis) {
  return kMicrosecondsPerMillisecond * timeMillis;
}

}  // namespace jazzlights

#endif  // JL_UTIL_TIME_H
