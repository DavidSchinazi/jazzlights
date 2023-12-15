#include "jazzlights/ui.h"
// This comment prevents automatic header reordering.

#include "jazzlights/config.h"

#if JL_IS_CONTROLLER(ATOM_LITE) || JL_IS_CONTROLLER(M5STAMP_PICO) || JL_IS_CONTROLLER(M5STAMP_C3U) || \
    JL_IS_CONTROLLER(ATOM_S3)

namespace jazzlights {

void arduinoUiInitialSetup(Player& /*player*/, Milliseconds /*currentTime*/) {}

void arduinoUiFinalSetup(Player& /*player*/, Milliseconds /*currentTime*/) {}

void arduinoUiLoop(Player& /*player*/, const Network& /*wifiNetwork*/, const Network& /*bleNetwork*/,
                   Milliseconds /*currentMillis*/) {}

#if JL_DEV
static constexpr uint8_t kBrightness = 2;
#elif JL_IS_CONFIG(STAFF)
static constexpr uint8_t kBrightness = 16;
#elif JL_IS_CONFIG(HAMMER)
static constexpr uint8_t kBrightness = 255;
#else
static constexpr uint8_t kBrightness = 32;
#endif

uint8_t getBrightness() { return kBrightness; }

}  // namespace jazzlights

#endif
