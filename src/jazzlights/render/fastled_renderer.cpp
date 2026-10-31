#include "jazzlights/render/fastled_renderer.h"

#ifdef ESP32

#include "jazzlights/util/log.h"

namespace jazzlights {

// These mirror what CFastLED::show() does.
void BeginFastLedFrame() { fl::EngineEvents::onBeginFrame(); }

void AddToFastLedFrame(CLEDController* controller, uint8_t brightness) {
  void* data = controller->beginShowLeds(controller->size());
  controller->showLedsInternal(brightness);
  controller->endShowLeds(data);
}

void EndFastLedFrame() {
  fl::EngineEvents::onEndFrame();
#if FASTLED_HAS_ENGINE_EVENTS
  fl::EngineEvents::onEndShowLeds();
#endif  // FASTLED_HAS_ENGINE_EVENTS
}

FastLedRenderer::FastLedRenderer(size_t numLeds, SetupFunction setupFunction)
    : numLeds_(numLeds), ledMemorySize_(numLeds_ * sizeof(CRGB)), setupFunction_(setupFunction) {
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

void FastLedRenderer::RenderPixel(size_t index, CRGB color) { ledsPlayer_[index] = color; }

uint32_t FastLedRenderer::GetPowerAtFullBrightness() const {
  return calculate_unscaled_power_mW(ledsPlayer_, numLeds_);
}

void FastLedRenderer::CopyLedsFromPlayerToLocked() {
  freshLedLockedDataAvailable_ = true;
  memcpy(ledsLocked_, ledsPlayer_, ledMemorySize_);
}
bool FastLedRenderer::CopyLedsFromLockedToFastLed() {
  const bool freshData = freshLedLockedDataAvailable_;
  if (freshData) {
    memcpy(ledsFastLed_, ledsLocked_, ledMemorySize_);
    freshLedLockedDataAvailable_ = false;
  }
  return freshData;
}

}  // namespace jazzlights

#endif  // ESP32
