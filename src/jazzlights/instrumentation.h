#include "jazzlights/config.h"
#include "jazzlights/util/time.h"

#ifndef JAZZLIGHTS_INSTRUMENTATION_H
#define JAZZLIGHTS_INSTRUMENTATION_H

#if JL_INSTRUMENTATION

namespace jazzlights {

#define ALL_TIME_POINTS \
  X(LoopStart) \
  X(Core2) \
  X(Buttons) \
  X(Bluetooth) \
  X(Player) \
  X(Brightness) \
  X(MainLED) \
  X(SecondLED) \
  X(NumTimePoints)  // Leave this as last member.

enum TimePoint {
#define X(v) k ## v,
  ALL_TIME_POINTS
#undef X
};

void saveTimePoint(TimePoint timePoint);

void printInstrumentationInfo(Milliseconds currentTime);

void ledWriteStart();
void ledWriteEnd();

}  // namespace jazzlights

#define SAVE_TIME_POINT(v) saveTimePoint(k ## v)

#else  // JL_INSTRUMENTATION

#define SAVE_TIME_POINT(v) do {} while (false)
#define ledWriteStart() do {} while (false)
#define ledWriteEnd() do {} while (false)

#endif  // JL_INSTRUMENTATION

#endif  // JAZZLIGHTS_INSTRUMENTATION_H
