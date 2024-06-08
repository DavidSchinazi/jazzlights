#ifndef JL_FASTLED_RENDERER_H
#define JL_FASTLED_RENDERER_H

#ifdef ARDUINO

#include <functional>
#include <memory>

#include "jazzlights/fastled_wrapper.h"
#include "jazzlights/renderer.h"

namespace jazzlights {

class FastLedRenderer : public Renderer {
 public:
  // 4 wires with specified data rate.
  template <ESPIChipsets CHIPSET, uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER, uint32_t SPI_DATA_RATE>
  static std::unique_ptr<FastLedRenderer> Create(size_t numLeds) {
    return std::unique_ptr<FastLedRenderer>(new FastLedRenderer(numLeds, [](CRGB* leds, size_t num) {
      return &FastLED.addLeds<CHIPSET, DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE>(leds, num);
    }));
  }

  // 4 wires with default data rate.
  template <ESPIChipsets CHIPSET, uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER>
  static std::unique_ptr<FastLedRenderer> Create(size_t numLeds) {
    return std::unique_ptr<FastLedRenderer>(new FastLedRenderer(numLeds, [](CRGB* leds, size_t num) {
      return &FastLED.addLeds<CHIPSET, DATA_PIN, CLOCK_PIN, RGB_ORDER>(leds, num);
    }));
  }

  // 3 wires.
  template <template <uint8_t DATA_PIN, EOrder RGB_ORDER> class CHIPSET, uint8_t DATA_PIN, EOrder RGB_ORDER>
  static std::unique_ptr<FastLedRenderer> Create(size_t numLeds) {
    return std::unique_ptr<FastLedRenderer>(new FastLedRenderer(
        numLeds, [](CRGB* leds, size_t num) { return &FastLED.addLeds<CHIPSET, DATA_PIN, RGB_ORDER>(leds, num); }));
  }

  void renderPixel(size_t index, CRGB color) override;

  uint32_t GetPowerAtFullBrightness() const;

  void copyLedsFromPlayerToLocked();
  bool copyLedsFromLockedToFastLed();

  void sendToLeds(uint8_t brightness = 255);

  void Setup() {
    // We use a lambda here in order to ensure that the call to FastLED.addLeds() happens on the FastLED task.
    // This ensures that the RMT interrupts are configured on the same CPU core as the one we write to the LEDs,
    // based on looking at FastLED's clockless_rmt_esp32.c and the header documentation for ESP32 esp_intr_alloc().
    ledController_ = setupFunction_(ledsFastLed_, numLeds_);
    setupFunction_ = nullptr;
  }

  ~FastLedRenderer();

 private:
  using SetupFunction = std::function<CLEDController*(CRGB*, size_t)>;
  explicit FastLedRenderer(size_t numLeds, SetupFunction setupFunction);

  const size_t numLeds_;
  const size_t ledMemorySize_;
  CRGB* ledsPlayer_;
  bool freshLedLockedDataAvailable_ = false;
  CRGB* ledsLocked_;  // Protected by FastLedRunner::lockedLedMutex_.
  CRGB* ledsFastLed_;
  CLEDController* ledController_ = nullptr;
  SetupFunction setupFunction_;
};

}  // namespace jazzlights

#endif  // ARDUINO

#endif  // JL_FASTLED_RENDERER_H
