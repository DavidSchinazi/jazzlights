#ifndef JL_UTIL_TIME_H
#define JL_UTIL_TIME_H

#include "jazzlights/types.h"

namespace jazzlights {

using Microseconds = int64_t;

JL_DEFINE_INT_TYPE(Milliseconds, int, int32_t);

// Get monotonically increasing time in microseconds.
Microseconds timeMicros();

// Convert time from milliseconds to microseconds.
Microseconds MillisecondsToMicroseconds(Milliseconds timeMillis);

long long MillisecondsSinceBootForLogging();

using FramesPerSecond = int32_t;

inline constexpr Microseconds kMicrosecondsPerMillisecond = 1000;
inline constexpr Microseconds kMicrosecondsPerSecond = 1000000;

}  // namespace jazzlights

#endif  // JL_UTIL_TIME_H
