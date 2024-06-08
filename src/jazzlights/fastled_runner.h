#ifndef JL_FASTLED_RUNNER_H
#define JL_FASTLED_RUNNER_H

#include "jazzlights/config.h"

#ifdef ARDUINO

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "jazzlights/fastled_renderer.h"
#include "jazzlights/fastled_wrapper.h"
#include "jazzlights/player.h"
#include "jazzlights/renderer.h"

#if JL_IS_CONTROLLER(ATOM_MATRIX)
#define JL_FASTLED_RUNNER_HAS_UI 1
#else  // ATOM_MATRIX
#define JL_FASTLED_RUNNER_HAS_UI 0
#endif  // ATOM_MATRIX

#define JL_FASTLED_INIT_ON_OUR_TASK 1

namespace jazzlights {

class FastLedRunner {
 public:
  explicit FastLedRunner(Player* player) : player_(player) {}

  // Disallow copy and move.
  FastLedRunner(const FastLedRunner&) = delete;
  FastLedRunner(FastLedRunner&&) = delete;
  FastLedRunner& operator=(const FastLedRunner&) = delete;
  FastLedRunner& operator=(FastLedRunner&&) = delete;

  // 4 wires with specified data rate.
  template <ESPIChipsets CHIPSET, uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER, uint32_t SPI_DATA_RATE>
  void AddLeds(const Layout& layout) {
    renderers_.emplace_back(
        FastLedRenderer::Create<CHIPSET, DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE>(layout.pixelCount()));
    player_->addStrand(layout, *renderers_.back());
  }

  // 4 wires with default data rate.
  template <ESPIChipsets CHIPSET, uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER>
  void AddLeds(const Layout& layout) {
    renderers_.emplace_back(FastLedRenderer::Create<CHIPSET, DATA_PIN, CLOCK_PIN, RGB_ORDER>(layout.pixelCount()));
    player_->addStrand(layout, *renderers_.back());
  }

  // 3 wires.
  template <template <uint8_t DATA_PIN, EOrder RGB_ORDER> class CHIPSET, uint8_t DATA_PIN, EOrder RGB_ORDER>
  void AddLeds(const Layout& layout) {
    renderers_.emplace_back(FastLedRenderer::Create<CHIPSET, DATA_PIN, RGB_ORDER>(layout.pixelCount()));
    player_->addStrand(layout, *renderers_.back());
  }

  void Render();
  void Start();

#if JL_FASTLED_RUNNER_HAS_UI
  template <template <uint8_t DATA_PIN, EOrder RGB_ORDER> class CHIPSET, uint8_t DATA_PIN, EOrder RGB_ORDER>
  void ConfigureUi(size_t uiNumLeds) {
    uiNumLeds_ = uiNumLeds;
    uiLedMemorySize_ = uiNumLeds_ * sizeof(CRGB);
    uiLedsPlayer_ = reinterpret_cast<CRGB*>(calloc(uiLedMemorySize_, 1));
    uiLedsLocked_ = reinterpret_cast<CRGB*>(calloc(uiLedMemorySize_, 1));
    uiLedsFastLed_ = reinterpret_cast<CRGB*>(calloc(uiLedMemorySize_, 1));
    if (uiLedsPlayer_ == nullptr || uiLedsLocked_ == nullptr || uiLedsFastLed_ == nullptr) {
      jll_fatal("Failed to allocate %zu*%zu", uiNumLeds_, sizeof(CRGB));
    }
    uiSetupFunction_ = [](CRGB* leds, size_t num) { return &FastLED.addLeds<CHIPSET, DATA_PIN, RGB_ORDER>(leds, num); };
  }

  void IngestUiPixels(CRGB* uiLeds, uint8_t uiBrightness) {
    uiFreshPlayer_ = true;
    uiBrightnessPlayer_ = uiBrightness;
    memcpy(uiLedsPlayer_, uiLeds, uiLedMemorySize_);
  }
#endif  // JL_FASTLED_RUNNER_HAS_UI

 private:
#if JL_FASTLED_RUNNER_HAS_UI
  void SetupUi() {
    uiController_ = uiSetupFunction_(uiLedsFastLed_, uiNumLeds_);
    uiSetupFunction_ = nullptr;
  }
#endif  // JL_FASTLED_RUNNER_HAS_UI
  void Setup();
  static void TaskFunction(void* parameters);
  void SendLedsToFastLed();

  std::vector<std::unique_ptr<FastLedRenderer>> renderers_;
  std::mutex lockedLedMutex_;
  uint8_t brightnessLocked_ = 0;  // Protected by lockedLedMutex_.
  TaskHandle_t taskHandle_ = nullptr;
  Player* player_;  // Unowned.

#if JL_FASTLED_RUNNER_HAS_UI
  CLEDController* uiController_ = nullptr;
  size_t uiNumLeds_ = 0;
  size_t uiLedMemorySize_ = 0;
  uint8_t uiBrightnessPlayer_ = 255;
  uint8_t uiBrightnessLocked_ = 255;  // Protected by lockedLedMutex_.
  bool uiFreshPlayer_ = false;
  bool uiFreshLocked_ = false;
  CRGB* uiLedsPlayer_ = nullptr;
  CRGB* uiLedsLocked_ = nullptr;  // Protected by lockedLedMutex_.
  CRGB* uiLedsFastLed_ = nullptr;
  using UiSetupFunction = std::function<CLEDController*(CRGB*, size_t)>;
  UiSetupFunction uiSetupFunction_ = nullptr;
#endif  // JL_FASTLED_RUNNER_HAS_UI
};

}  // namespace jazzlights

#endif  // ARDUINO

#endif  // JL_FASTLED_RUNNER_H
