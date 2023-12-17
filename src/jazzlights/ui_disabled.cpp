#include "jazzlights/ui.h"
// This comment prevents automatic header reordering.

#include "jazzlights/config.h"

#ifdef ARDUINO

namespace jazzlights {
namespace {
#if JL_DEV
static constexpr uint8_t kBrightness = 2;
#elif JL_IS_CONFIG(STAFF)
static constexpr uint8_t kBrightness = 16;
#elif JL_IS_CONFIG(HAMMER)
static constexpr uint8_t kBrightness = 255;
#else
static constexpr uint8_t kBrightness = 32;
#endif
}  // namespace

void NoOpUi::InitialSetup(Milliseconds /*currentTime*/) { player_.SetBrightness(kBrightness); }

}  // namespace jazzlights

#endif  // ARDUINO
