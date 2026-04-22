#ifndef JAZZLIGHTS_UI_AUDIO_H
#define JAZZLIGHTS_UI_AUDIO_H

#include "jazzlights/config.h"
#include "jazzlights/ui/ui.h"

#ifdef ESP32

#if JL_AUDIO_VISUALIZER

#include <mutex>
#include <vector>

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
  int kScreenWidth;
  int kScreenHeight;
  enum class VisualizationMode { kSpectrum, kWaveform };
  VisualizationMode visualization_mode_ = VisualizationMode::kSpectrum;
  std::vector<float> waveform_buffer_;
  std::vector<bool> beat_buffer_;
  int waveform_index_ = 0;
  double last_waveform_update_ = 0;
  bool showing_no_audio_data_ = false;
  bool showing_squelch_ = false;
};

}  // namespace jazzlights

#else  // JL_AUDIO_VISUALIZER

namespace jazzlights {

class AudioVisualizerUi : public Esp32Ui {
 public:
  explicit AudioVisualizerUi(Player& player) : Esp32Ui(player) {}
  void InitialSetup() override {}
  void FinalSetup() override {}
  void RunLoop(Milliseconds /*currentTime*/) override {}
};

}  // namespace jazzlights

#endif  // JL_AUDIO_VISUALIZER

#endif  // ESP32

#endif  // JAZZLIGHTS_UI_AUDIO_H
