#ifndef JAZZLIGHTS_VEST_H
#define JAZZLIGHTS_VEST_H

#include "jazzlights/config.h"

#ifdef ARDUINO

#include <string>

#include "jazzlights/player.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

void vestSetup(void);
void vestLoop(void);

}  // namespace jazzlights

#endif  // ARDUINO

#endif  // JAZZLIGHTS_VEST_H
