#ifndef JL_PRIMARY_RUNLOOP_H
#define JL_PRIMARY_RUNLOOP_H

#include "jazzlights/util/config.h"

#ifdef ESP32

#include "jazzlights/render/player.h"

namespace jazzlights {

Player* GetPrimaryPlayer();
void SetupPrimaryRunLoop();
void RunPrimaryRunLoop();

}  // namespace jazzlights

#endif  // ESP32

#endif  // JL_PRIMARY_RUNLOOP_H
