#include "jazzlights/config.h"
#include "jazzlights/util/time.h"

#ifndef JAZZLIGHTS_INSTRUMENTATION_H
#define JAZZLIGHTS_INSTRUMENTATION_H

#if JL_INSTRUMENTATION

namespace jazzlights {

void printInstrumentationInfo(Milliseconds currentTime);

}  // namespace jazzlights

#endif  // JL_INSTRUMENTATION

#endif  // JAZZLIGHTS_INSTRUMENTATION_H
