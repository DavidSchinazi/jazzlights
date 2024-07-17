#ifndef JL_PRIMARY_RUNLOOP_H
#define JL_PRIMARY_RUNLOOP_H

#include "jazzlights/config.h"

#ifdef ESP32

namespace jazzlights {

void SetupPrimaryRunLoop();
void RunPrimaryRunLoop();

}  // namespace jazzlights

#endif  // ESP32

#endif  // JL_PRIMARY_RUNLOOP_H
