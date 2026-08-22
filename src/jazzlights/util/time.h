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
  constexpr OptionalMicroseconds() noexcept : inner_(kPresentBit) {
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
  constexpr OptionalMicroseconds(std::nullopt_t) noexcept : inner_(kPresentBit) {}
  constexpr OptionalMicroseconds(Microseconds micros) noexcept : inner_(CheckValue(micros)) {}
  constexpr OptionalMicroseconds(const OptionalMicroseconds& other) noexcept = default;
  constexpr OptionalMicroseconds(OptionalMicroseconds&& other) noexcept = default;
  constexpr OptionalMicroseconds& operator=(const OptionalMicroseconds& other) = default;
  constexpr OptionalMicroseconds& operator=(OptionalMicroseconds&& other) = default;
  constexpr OptionalMicroseconds& operator=(Microseconds micros) noexcept {
    inner_ = CheckValue(micros);
    return *this;
  }
  constexpr OptionalMicroseconds& operator=(std::nullopt_t) noexcept {
    inner_ = kPresentBit;
    return *this;
  }
  constexpr Microseconds value() const noexcept { return CheckValue(inner_); }
  constexpr Microseconds operator*() const noexcept { return value(); }

  constexpr bool has_value() const noexcept { return (inner_ & kPresentBit) == 0; }
  constexpr explicit operator bool() const noexcept { return has_value(); }

  constexpr Microseconds value_or(Microseconds default_micros) const noexcept {
    if (has_value()) {
      return value();
    } else {
      return default_micros;
    }
  }

  constexpr bool operator==(const OptionalMicroseconds& other) { return inner_ == other.inner_; }
  constexpr bool operator!=(const OptionalMicroseconds& other) { return inner_ != other.inner_; }
  constexpr bool operator==(std::nullopt_t) { return !has_value(); }
  constexpr bool operator!=(std::nullopt_t) { return has_value(); }

  constexpr void reset() noexcept { inner_ = kPresentBit; }

  void swap(OptionalMicroseconds& other) noexcept { std::swap(inner_, other.inner_); }

 private:
  static inline constexpr uint64_t kPresentBit = 0x4000000000000000ULL;

  static constexpr Microseconds CheckValue(Microseconds micros) noexcept {
#if JL_OPT_US_DEBUG
    if ((micros & kPresentBit) != 0) { abort(); }
#endif  // JL_OPT_US_DEBUG
    return micros;
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
