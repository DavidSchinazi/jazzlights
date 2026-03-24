#ifndef JAZZLIGHTS_UI_AUDIO_H
#define JAZZLIGHTS_UI_AUDIO_H

#include "jazzlights/config.h"
#include "jazzlights/ui/ui.h"

#ifdef ESP32

#if JL_AUDIO_VISUALIZER && JL_IS_CONTROLLER(CORES3)

#include <mutex>

#include "jazzlights/audio.h"

namespace jazzlights {

class AudioVisualizerUi : public Esp32Ui {
 public:
  explicit AudioVisualizerUi(Player& player);
  ~AudioVisualizerUi() override = default;
  void InitialSetup() override;
  void FinalSetup() override;
  void RunLoop(Milliseconds currentTime) override;

 private:
  static constexpr int kScreenWidth = 320;
  static constexpr int kScreenHeight = 240;
  enum class VisualizationMode { kSpectrum, kWaveform };
  VisualizationMode visualization_mode_ = VisualizationMode::kSpectrum;
  float waveform_buffer_[kScreenWidth] = {0};
  bool beat_buffer_[kScreenWidth] = {false};
  int waveform_index_ = 0;
  double last_waveform_update_ = 0;
};

}  // namespace jazzlights

#else  // JL_AUDIO_VISUALIZER && JL_IS_CONTROLLER(CORES3)

namespace jazzlights {

class AudioVisualizerUi : public Esp32Ui {
 public:
  explicit AudioVisualizerUi(Player& player) : Esp32Ui(player) {}
  void InitialSetup() override {}
  void FinalSetup() override {}
  void RunLoop(Milliseconds /*currentTime*/) override {}
};

}  // namespace jazzlights

#endif  // JL_AUDIO_VISUALIZER && JL_IS_CONTROLLER(CORES3)

#endif  // ESP32

#endif  // JAZZLIGHTS_UI_AUDIO_H
