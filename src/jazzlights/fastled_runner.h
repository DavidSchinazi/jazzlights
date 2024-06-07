#ifndef JL_FASTLED_RUNNER_H
#define JL_FASTLED_RUNNER_H

#ifdef ARDUINO

#include <memory>
#include <mutex>
#include <vector>

#include "jazzlights/fastled_renderer.h"
#include "jazzlights/fastled_wrapper.h"
#include "jazzlights/player.h"
#include "jazzlights/renderer.h"

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

 private:
  static void TaskFunction(void* parameters);
  void SendLedsToFastLed();

  std::vector<std::unique_ptr<FastLedRenderer>> renderers_;
  std::mutex lockedLedMutex_;
  uint8_t brightnessLocked_ = 0;  // Protected by lockedLedMutex_.
  TaskHandle_t taskHandle_ = nullptr;
  Player* player_;  // Unowned.
};

}  // namespace jazzlights

#endif  // ARDUINO

#endif  // JL_FASTLED_RUNNER_H
