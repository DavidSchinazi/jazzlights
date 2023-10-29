#ifndef JAZZLIGHTS_FASTLED_RENDERER_H
#define JAZZLIGHTS_FASTLED_RENDERER_H

#ifdef ARDUINO

#include <memory>

#include "jazzlights/fastled_wrapper.h"
#include "jazzlights/renderer.h"

namespace jazzlights {

class FastLedRenderer : public Renderer {
 public:
  // 4 wires with specified data rate.
  template <ESPIChipsets CHIPSET, uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER, uint32_t SPI_DATA_RATE>
  static std::unique_ptr<FastLedRenderer> Create(size_t numLeds) {
    std::unique_ptr<FastLedRenderer> r = std::unique_ptr<FastLedRenderer>(new FastLedRenderer(numLeds));
    r->ledController_ =
        &FastLED.addLeds<CHIPSET, DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE>(r->ledsFastLed_, numLeds);
    return r;
  }

  // 4 wires with default data rate.
  template <ESPIChipsets CHIPSET, uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER>
  static std::unique_ptr<FastLedRenderer> Create(size_t numLeds) {
    std::unique_ptr<FastLedRenderer> r = std::unique_ptr<FastLedRenderer>(new FastLedRenderer(numLeds));
    r->ledController_ = &FastLED.addLeds<CHIPSET, DATA_PIN, CLOCK_PIN, RGB_ORDER>(r->ledsFastLed_, numLeds);
    return r;
  }

  // 3 wires.
  template <template <uint8_t DATA_PIN, EOrder RGB_ORDER> class CHIPSET, uint8_t DATA_PIN, EOrder RGB_ORDER>
  static std::unique_ptr<FastLedRenderer> Create(size_t numLeds) {
    std::unique_ptr<FastLedRenderer> r = std::unique_ptr<FastLedRenderer>(new FastLedRenderer(numLeds));
    r->ledController_ = &FastLED.addLeds<CHIPSET, DATA_PIN, RGB_ORDER>(r->ledsFastLed_, numLeds);
    return r;
  }

  void render(InputStream<Color>& colors) override;

  uint32_t GetPowerAtFullBrightness() const;

  void copyLedsFromPlayerToLocked();
  bool copyLedsFromLockedToFastLed();

  void sendToLeds(uint8_t brightness = 255);

  ~FastLedRenderer();

 private:
  explicit FastLedRenderer(size_t numLeds);

  size_t ledMemorySize_;
  CRGB* ledsPlayer_;
  bool freshLedLockedDataAvailable_ = false;
  CRGB* ledsLocked_;  // Protected by gLockedLedMutex.
  CRGB* ledsFastLed_;
  CLEDController* ledController_;
};

}  // namespace jazzlights

#endif  // ARDUINO

#endif  // JAZZLIGHTS_FASTLED_RENDERER_H
