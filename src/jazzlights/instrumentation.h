#ifndef JL_INSTRUMENTATION_H
#define JL_INSTRUMENTATION_H

#include <cstddef>

#include "jazzlights/config.h"
#include "jazzlights/util/time.h"

#if JL_TIMING
#include <freertos/FreeRTOS.h>

#include "jazzlights/util/log.h"
#endif  // JL_TIMING

namespace jazzlights {

#if JL_INSTRUMENTATION || JL_TIMING
void printInstrumentationInfo(Milliseconds currentTime);
#else   // JL_INSTRUMENTATION || JL_TIMING
inline void printInstrumentationInfo(Milliseconds /*currentTime*/) {}
#endif  // JL_INSTRUMENTATION || JL_TIMING

#if JL_TIMING

// Add new time point categories here:

#define PRIMARY_RUNLOOP_TIME_POINTS(n) \
  X(n, LoopStart)                      \
  X(n, UserInterface)                  \
  X(n, Bluetooth)                      \
  X(n, PlayerCompute)                  \
  X(n, Brightness)                     \
  X(n, GetLock)                        \
  X(n, Copy)                           \
  X(n, Notify)                         \
  X(n, Yield)                          \
  X(n, LoopEnd)

#define FASTLED_TIME_POINTS(n) \
  X(n, Start)                  \
  X(n, GetLock)                \
  X(n, Copy)                   \
  X(n, WaitForNotify)          \
  X(n, WriteToLeds)

#define ALL_TIME_POINT_ENUMS                     \
  Y(PrimaryRunLoop, PRIMARY_RUNLOOP_TIME_POINTS) \
  Y(FastLed, FASTLED_TIME_POINTS)

// Internal implementation of time points.

#define SETUP_TIME_POINT_ENUM(NAME, ALL_TIME_POINTS)   \
  enum class NAME##TimePoint{                          \
      ALL_TIME_POINTS(NAME##TimePoint) kNumTimePoints, \
  };

#define SETUP_TIME_POINT_TO_STR(NAME, ALL_TIME_POINTS)              \
  inline const char* TimePointToString(NAME##TimePoint timePoint) { \
    switch (timePoint) {                                            \
      ALL_TIME_POINTS(NAME##TimePoint)                              \
      case NAME##TimePoint::kNumTimePoints: return #NAME;           \
    }                                                               \
    return "Unknown";                                               \
  }

#define X(e, v) k##v,
#define Y(n, a) SETUP_TIME_POINT_ENUM(n, a)
ALL_TIME_POINT_ENUMS
#undef Y
#undef X

// Apparently different version of clang-format disagree on this line.
// clang-format off
#define X(e, v) case e::k##v: return #v;
// clang-format on
#define Y(n, a) SETUP_TIME_POINT_TO_STR(n, a)
ALL_TIME_POINT_ENUMS
#undef Y
#undef X

template <typename ENUM>
class TimePointSaver {
 public:
  void PrintAndClearTimePoints() {
    int64_t totalTimePointsSum = 0;
    for (size_t i = 0; i < static_cast<size_t>(ENUM::kNumTimePoints); i++) {
      totalTimePointsSum += timePointDatas_[i].sumTimes;
    }
    const int64_t minPercentOffset = totalTimePointsSum / 200;
    jll_info("%s:", TimePointToString(ENUM::kNumTimePoints));
    for (size_t i = 0; i < static_cast<size_t>(ENUM::kNumTimePoints); i++) {
      jll_info("%15s: %2lld%% %8lld", TimePointToString(static_cast<ENUM>(i)),
               (timePointDatas_[i].sumTimes * 100 + minPercentOffset) / totalTimePointsSum,
               timePointDatas_[i].sumTimes);
    }
    for (size_t i = 0; i < static_cast<size_t>(ENUM::kNumTimePoints); i++) { timePointDatas_[i] = TimePointData(); }
  }

  void SaveTimePoint(ENUM timePoint) {
    TimePointData& thisTimePointData = timePointDatas_[static_cast<size_t>(timePoint)];
    const TimePointData& prevTimePointData = static_cast<size_t>(timePoint) > 0
                                                 ? timePointDatas_[static_cast<size_t>(timePoint) - 1]
                                                 : timePointDatas_[static_cast<size_t>(ENUM::kNumTimePoints) - 1];
    thisTimePointData.lastSavedTime = esp_timer_get_time();
    if (prevTimePointData.lastSavedTime >= 0) {
      thisTimePointData.sumTimes += thisTimePointData.lastSavedTime - prevTimePointData.lastSavedTime;
    }
  }

  static TimePointSaver<ENUM>* Get() {
    static TimePointSaver<ENUM> sTimePointSaver;
    return &sTimePointSaver;
  }

 private:
  struct TimePointData {
    int64_t lastSavedTime = -1;
    int64_t sumTimes = 0;
  };

  TimePointData timePointDatas_[static_cast<size_t>(ENUM::kNumTimePoints)];
};

#define SAVE_TIME_POINT(e, v) TimePointSaver<e##TimePoint>::Get()->SaveTimePoint(e##TimePoint::k##v)

#define ALL_COUNT_POINTS \
  X(LedPrintLoop)        \
  X(LedPrintSend)        \
  X(NumCountPoints)  // Leave this as last member.

enum CountPoint {
#define X(v) k##v,
  ALL_COUNT_POINTS
#undef X
};

void saveCountPoint(CountPoint countPoint);

#define SAVE_COUNT_POINT(v) saveCountPoint(k##v)

void ledWriteStart();
void ledWriteEnd();

#else  // JL_TIMING

#define SAVE_TIME_POINT(e, v) \
  do {                        \
  } while (false)
inline void ledWriteStart() {}
inline void ledWriteEnd() {}

#define SAVE_COUNT_POINT(v) \
  do {                      \
  } while (false)

#endif  // JL_TIMING

}  // namespace jazzlights

#endif  // JL_INSTRUMENTATION_H
