#ifndef JAZZLIGHTS_UI_AUDIO_H
#define JAZZLIGHTS_UI_AUDIO_H

#include "jazzlights/config.h"
#include "jazzlights/ui/ui.h"

#if JL_AUDIO_VISUALIZER && JL_IS_CONTROLLER(CORES3)

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <mutex>

namespace jazzlights {

class AudioVisualizerUi : public Esp32Ui {
 public:
  explicit AudioVisualizerUi(Player& player);
  ~AudioVisualizerUi() override;
  void InitialSetup() override;
  void FinalSetup() override;
  void RunLoop(Milliseconds currentTime) override;

 private:
  static void AudioTask(void* param);
  static constexpr int kNumBands = 32;
  static constexpr int kScreenWidth = 320;
  static constexpr int kScreenHeight = 240;
  enum class VisualizationMode { kSpectrum, kWaveform };
  VisualizationMode visualization_mode_ = VisualizationMode::kSpectrum;
  float waveform_buffer_[kScreenWidth] = {0};
  int waveform_index_ = 0;
  float band_magnitudes_[kNumBands] = {0};
  float peak_magnitudes_[kNumBands] = {0};
  std::mutex audio_data_mutex_;
  TaskHandle_t audio_task_handle_ = nullptr;
  int16_t* audio_buffer_ = nullptr;
  float* fft_input_ = nullptr;
  float* fft_output_ = nullptr;
  float* fft_window_ = nullptr;
  Milliseconds last_peak_decay_time_ = 0;
  float agc_min_ = 50.0f;
  float agc_max_ = 90.0f;
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

#endif  // JAZZLIGHTS_UI_AUDIO_H
