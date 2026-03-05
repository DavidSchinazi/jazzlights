#ifndef JAZZLIGHTS_UI_AUDIO_H
#define JAZZLIGHTS_UI_AUDIO_H

#include "jazzlights/ui/ui.h"

#ifdef ESP32
#include <mutex>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace jazzlights {

class AudioVisualizerUi : public Esp32Ui {
 public:
  explicit AudioVisualizerUi(Player& player);
  ~AudioVisualizerUi() override;
  void InitialSetup() override;
  void FinalSetup() override;
  void RunLoop(Milliseconds currentTime) override;

 private:
#if JL_AUDIO_VISUALIZER && JL_IS_CONTROLLER(CORES3)
  static void AudioTask(void* param);
#endif
  static constexpr int kNumBands = 32;
  float band_magnitudes_[kNumBands] = {0};
  float peak_magnitudes_[kNumBands] = {0};
#if JL_AUDIO_VISUALIZER && JL_IS_CONTROLLER(CORES3)
  std::mutex audio_data_mutex_;
  TaskHandle_t audio_task_handle_ = nullptr;
  int16_t* audio_buffer_ = nullptr;
  float* fft_input_ = nullptr;
  float* fft_output_ = nullptr;
  float* fft_window_ = nullptr;
#endif
  Milliseconds last_peak_decay_time_ = 0;
};

} // namespace jazzlights

#endif // JAZZLIGHTS_UI_AUDIO_H
