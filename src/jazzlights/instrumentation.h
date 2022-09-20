#include "jazzlights/config.h"
#include "jazzlights/util/time.h"

#ifndef JAZZLIGHTS_INSTRUMENTATION_H
#define JAZZLIGHTS_INSTRUMENTATION_H

namespace jazzlights {

#if JL_INSTRUMENTATION || JL_TIMING
void printInstrumentationInfo(Milliseconds currentTime);
#else   // JL_INSTRUMENTATION || JL_TIMING
inline void printInstrumentationInfo(Milliseconds /*currentTime*/) {}
#endif  // JL_INSTRUMENTATION || JL_TIMING

#if JL_TIMING

#define ALL_TIME_POINTS \
  X(LoopStart)          \
  X(Core2)              \
  X(Buttons)            \
  X(Bluetooth)          \
  X(Player)             \
  X(Brightness)         \
  X(MainLED)            \
  X(SecondLED)          \
  X(NumTimePoints)  // Leave this as last member.

enum TimePoint {
#define X(v) k##v,
  ALL_TIME_POINTS
#undef X
};

void saveTimePoint(TimePoint timePoint);

void ledWriteStart();
void ledWriteEnd();

#define SAVE_TIME_POINT(v) saveTimePoint(k##v)

#else  // JL_TIMING

#define SAVE_TIME_POINT(v) \
  do {                     \
  } while (false)
inline void ledWriteStart() {}
inline void ledWriteEnd() {}

#endif  // JL_TIMING

}  // namespace jazzlights

#endif  // JAZZLIGHTS_INSTRUMENTATION_H
