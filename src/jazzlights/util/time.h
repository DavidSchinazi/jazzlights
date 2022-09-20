#ifndef JAZZLIGHTS_TIME_H
#define JAZZLIGHTS_TIME_H
#include <stdint.h>

namespace jazzlights {

typedef int32_t Milliseconds;

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

#endif  // JAZZLIGHTS_TIME_H
