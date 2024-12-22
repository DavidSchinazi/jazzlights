#ifndef JL_UTIL_TIME_H
#define JL_UTIL_TIME_H

#include "jazzlights/types.h"

namespace jazzlights {

JL_DEFINE_INT_TYPE(Milliseconds, int, int32_t);

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
