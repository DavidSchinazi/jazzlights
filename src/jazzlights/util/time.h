#ifndef JL_UTIL_TIME_H
#define JL_UTIL_TIME_H
#include <stdint.h>

namespace jazzlights {

// We would normally define Milliseconds as int32_t, but then some compilers would require us to use PRId32 as a printf
// format, so we define it as int so we can use %d instead.
typedef int Milliseconds;
static_assert(sizeof(int32_t) == sizeof(Milliseconds), "bad int size");

enum : Milliseconds {
  ONE_SECOND = 1000,
  ONE_MINUTE = 60 * ONE_SECOND,
  MAX_TIME = 2147483647,
};

/**
 * Get monotonically increasing time in milliseconds
 */
Milliseconds timeMillis();

typedef int32_t FramesPerSecond;

}  // namespace jazzlights

#endif  // JL_UTIL_TIME_H
