#ifndef JL_UTIL_TIME_H
#define JL_UTIL_TIME_H

#include <climits>
#include <limits>
#include <optional>

#include "jazzlights/types.h"

namespace jazzlights {

using Microseconds = int64_t;

#ifndef JL_OPT_US_DEBUG
#if JL_DEV || JL_DEBUG
#define JL_OPT_US_DEBUG 1
#else  // JL_DEV || JL_DEBUG
#define JL_OPT_US_DEBUG 0
#endif  // JL_DEV || JL_DEBUG
#endif  // JL_OPT_US_DEBUG

// OptionalMicroseconds is intended to be similar in interface to std::optional<Microseconds>. It uses the bit before
// the sign bit bit to mark the optional status to save memory. The assumption is that we'll never need all 64 bits for
// microsecond timestamps, as 62 bits can represent over a hundred thousand years in microseconds.
class OptionalMicroseconds {
 public:
  OptionalMicroseconds() : inner_(kPresentBit) {
    // These are sufficiently guaranteed in C++20, but our codebase supports C++17 so we double-check.
    static_assert(CHAR_BIT == 8, "Silly platform");
    static_assert(std::numeric_limits<Microseconds>::min() == -std::numeric_limits<Microseconds>::max() - 1,
                  "Bad two's complement limits");
    static_assert(sizeof(Microseconds) * CHAR_BIT == std::numeric_limits<Microseconds>::digits + 1,
                  "Bad two's complement number of bits");
    static_assert(sizeof(Microseconds) == sizeof(inner_), "bad type size");
    static_assert(std::numeric_limits<Microseconds>::min() == std::numeric_limits<decltype(inner_)>::min(),
                  "bad type min");
    static_assert(std::numeric_limits<Microseconds>::max() == std::numeric_limits<decltype(inner_)>::max(),
                  "bad type max");
  }
  OptionalMicroseconds(Microseconds micros) { AssignInput(micros); }
  OptionalMicroseconds& operator=(Microseconds micros) {
    AssignInput(micros);
    return *this;
  }
  Microseconds operator*() const noexcept { return GetOutput(); }
  operator bool() const noexcept { return (inner_ & kPresentBit) == 0; }
  void reset() { inner_ = kPresentBit; }

 private:
  static inline constexpr uint64_t kPresentBit = 0x4000000000000000ULL;

  void AssignInput(Microseconds micros) {
#if JL_OPT_US_DEBUG
    if ((micros & kPresentBit) != 0) { abort(); }
#endif  // JL_OPT_US_DEBUG
    inner_ = micros;
  }

  Microseconds GetOutput() const noexcept {
#if JL_OPT_US_DEBUG
    if ((inner_ & kPresentBit) != 0) { abort(); }
#endif  // JL_OPT_US_DEBUG
    return inner_;
  }

  Microseconds inner_;
};

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
