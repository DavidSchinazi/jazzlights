#include "jazzlights/fastled_renderer.h"

#ifdef ARDUINO

#include "jazzlights/util/log.h"

namespace jazzlights {

FastLedRenderer::FastLedRenderer(size_t numLeds) : ledMemorySize_(numLeds * sizeof(CRGB)) {
  ledsPlayer_ = reinterpret_cast<CRGB*>(calloc(ledMemorySize_, 1));
  ledsLocked_ = reinterpret_cast<CRGB*>(calloc(ledMemorySize_, 1));
  ledsFastLed_ = reinterpret_cast<CRGB*>(calloc(ledMemorySize_, 1));
  if (ledsPlayer_ == nullptr || ledsLocked_ == nullptr || ledsFastLed_ == nullptr) {
    jll_fatal("Failed to allocate %zu*%zu", numLeds, sizeof(CRGB));
  }
}

FastLedRenderer::~FastLedRenderer() {
  free(ledsPlayer_);
  free(ledsLocked_);
  free(ledsFastLed_);
}

void FastLedRenderer::renderPixel(size_t index, CRGB color) { ledsPlayer_[index] = color; }

uint32_t FastLedRenderer::GetPowerAtFullBrightness() const {
  return calculate_unscaled_power_mW(ledController_->leds(), ledController_->size());
}

void FastLedRenderer::copyLedsFromPlayerToLocked() {
  freshLedLockedDataAvailable_ = true;
  memcpy(ledsLocked_, ledsPlayer_, ledMemorySize_);
}
bool FastLedRenderer::copyLedsFromLockedToFastLed() {
  const bool freshData = freshLedLockedDataAvailable_;
  if (freshData) {
    memcpy(ledsFastLed_, ledsLocked_, ledMemorySize_);
    freshLedLockedDataAvailable_ = false;
  }
  return freshData;
}

void FastLedRenderer::sendToLeds(uint8_t brightness) { ledController_->showLeds(brightness); }

}  // namespace jazzlights

#endif  // ARDUINO
