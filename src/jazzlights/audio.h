#ifndef JAZZLIGHTS_AUDIO_H
#define JAZZLIGHTS_AUDIO_H

#include "jazzlights/config.h"

#if JL_AUDIO_VISUALIZER

#include <driver/i2s_types.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdint>
#include <mutex>

#include "jazzlights/util/time.h"

namespace jazzlights {

class Audio {
 public:
  static Audio& Get();
  void Setup();

  static constexpr int kNumBands = 32;

  struct VisualizerData {
    float bands[kNumBands];
    float peaks[kNumBands];
    float agc_min;
    float agc_max;
    float volume;
    bool beat;
    Milliseconds last_read_time;
  };

  void GetVisualizerData(VisualizerData* data);

  bool IsAgcEnabled() const { return agc_enabled_; }
  void SetAgcEnabled(bool enabled) { agc_enabled_ = enabled; }

 private:
  Audio() = default;
  static void AudioTask(void* param);
  void Initialize();
  void ReadAndProcessAudio();

  std::mutex audio_data_mutex_;
  TaskHandle_t audio_task_handle_ = nullptr;
  i2s_chan_handle_t rx_handle_ = nullptr;

  float band_magnitudes_[kNumBands] = {0};
  float peak_magnitudes_[kNumBands] = {0};
  float agc_min_ = 40.0f;
  float agc_max_ = 100.0f;
  bool agc_enabled_ = false;
  float squelch_threshold_ = 75.0f;
  float volume_ = 0;
  bool beat_ = false;
  uint32_t last_beat_time_ = 0;
  float prev_bands_[8] = {0};
  float prev_sample_ = 0;
  Milliseconds last_read_time_ = -1;

  int16_t* audio_buffer_ = nullptr;
  float* fft_input_ = nullptr;
  float* fft_output_ = nullptr;
  float* fft_window_ = nullptr;

  static constexpr int kAgcWindowSize = 312;  // ~5 seconds at 16ms per sample
  float agc_buffer_[kAgcWindowSize] = {0};
  int agc_index_ = 0;

  static constexpr int kBeatWindowSize = 60;  // ~1 second at 16ms per sample
  float beat_buffer_[kBeatWindowSize] = {0};
  int beat_index_ = 0;
};

}  // namespace jazzlights

#endif  // JL_AUDIO_VISUALIZER

#endif  // JAZZLIGHTS_AUDIO_H
