#include "jazzlights/ui/ui_disabled.h"

#include "jazzlights/config.h"

#ifdef ESP32

namespace jazzlights {
namespace {
#if JL_DEV
static constexpr uint8_t kBrightness = 2;
#elif JL_IS_CONFIG(STAFF)
static constexpr uint8_t kBrightness = 16;
#elif JL_IS_CONFIG(HAMMER)
static constexpr uint8_t kBrightness = 255;
#elif JL_IS_CONFIG(CREATURE)
static constexpr uint8_t kBrightness = 96;
#else
static constexpr uint8_t kBrightness = 32;
#endif
}  // namespace

void NoOpUi::InitialSetup(Milliseconds /*currentTime*/) { player_.set_brightness(kBrightness); }

}  // namespace jazzlights

#endif  // ESP32
