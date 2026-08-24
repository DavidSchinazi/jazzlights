#ifndef JL_AUDIO_AUDIO_H
#define JL_AUDIO_AUDIO_H

#include "jazzlights/util/config.h"

#if JL_AUDIO_VISUALIZER

#include <driver/i2s_types.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdint>
#include <mutex>
#include <optional>

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
    float agcMin;
    float agcMax;
    float volume;
    bool beat;
    bool squelch;
    OptionalMicroseconds lastReadTime;
  };

  void GetVisualizerData(VisualizerData* data);

  bool IsAgcEnabled() const { return agcEnabled_; }
  void SetAgcEnabled(bool enabled) { agcEnabled_ = enabled; }

 private:
  Audio() = default;
  static void AudioTask(void* param);
  void Initialize();
  void ReadAndProcessAudio();

  std::mutex audioDataMutex_;
  TaskHandle_t audioTaskHandle_ = nullptr;
  i2s_chan_handle_t rxHandle_ = nullptr;

  float bandMagnitudes_[kNumBands] = {0};
  float peakMagnitudes_[kNumBands] = {0};
  float agcMin_ = 40.0f;
  float agcMax_ = 100.0f;
  bool agcEnabled_ = false;
  float squelchThreshold_ = 75.0f;
  float volume_ = 0;
  bool beat_ = false;
  bool isSquelched_ = false;
  Microseconds lastBeatTime_ = 0;
  float prevBands_[8] = {0};
  float prevSample_ = 0;
  OptionalMicroseconds lastReadTime_;

  int16_t* audioBuffer_ = nullptr;
  float* fftInput_ = nullptr;
  float* fftOutput_ = nullptr;
  float* fftWindow_ = nullptr;

  static constexpr int kAgcWindowSize = 312;  // ~5 seconds at 16ms per sample
  float agcBuffer_[kAgcWindowSize] = {0};
  int agcIndex_ = 0;

  static constexpr int kBeatWindowSize = 60;  // ~1 second at 16ms per sample
  float beatBuffer_[kBeatWindowSize] = {0};
  int beatIndex_ = 0;
};

}  // namespace jazzlights

#endif  // JL_AUDIO_VISUALIZER

#endif  // JL_AUDIO_AUDIO_H
