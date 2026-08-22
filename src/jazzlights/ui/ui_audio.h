#ifndef JAZZLIGHTS_UI_AUDIO_H
#define JAZZLIGHTS_UI_AUDIO_H

#include "jazzlights/config.h"
#include "jazzlights/ui/ui.h"

#ifdef ESP32

#if JL_AUDIO_VISUALIZER && (JL_IS_CONTROLLER(CORES3) || JL_IS_CONTROLLER(CORE2AWS) || JL_IS_CONTROLLER(M5STICK_C))

#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

#include "jazzlights/audio.h"

namespace jazzlights {

class AudioVisualizerUi : public Esp32Ui {
 public:
  explicit AudioVisualizerUi(Player& player);
  ~AudioVisualizerUi() override = default;
  void InitialSetup() override;
  void FinalSetup() override;
  void RunLoop() override;

 private:
  int screen_width_;
  int screen_height_;
  enum class VisualizationMode { kMenu, kSpectrum, kWaveform, kBrightnessKeypad, kPaletteMenu };
  VisualizationMode visualization_mode_ = VisualizationMode::kSpectrum;
  int32_t keypad_value_ = 0;
  bool keypad_has_value_ = false;
  std::vector<float> waveform_buffer_;
  std::vector<bool> beat_buffer_;
  int waveform_index_ = 0;
  OptionalMicroseconds last_waveform_update_;
  bool showing_no_audio_data_ = false;
  bool showing_squelch_ = false;
};

}  // namespace jazzlights

#else  // JL_AUDIO_VISUALIZER && (CORES3 || CORE2AWS || M5STICK_C)

namespace jazzlights {

class AudioVisualizerUi : public Esp32Ui {
 public:
  explicit AudioVisualizerUi(Player& player) : Esp32Ui(player) {}
  void InitialSetup() override {}
  void FinalSetup() override {}
  void RunLoop() override {}
};

}  // namespace jazzlights

#endif  // JL_AUDIO_VISUALIZER && (CORES3 || CORE2AWS || M5STICK_C)

#endif  // ESP32

#endif  // JAZZLIGHTS_UI_AUDIO_H
