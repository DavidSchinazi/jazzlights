#ifndef JAZZLIGHTS_UI_AUDIO_H
#define JAZZLIGHTS_UI_AUDIO_H

#include "jazzlights/ui/ui.h"

namespace jazzlights {

class AudioVisualizerUi : public Esp32Ui {
 public:
  explicit AudioVisualizerUi(Player& player);
  void InitialSetup() override;
  void FinalSetup() override;
  void RunLoop(Milliseconds currentTime) override;

 private:
  static constexpr int kNumBands = 32;
  float band_magnitudes_[kNumBands];
  float peak_magnitudes_[kNumBands];
  Milliseconds last_peak_decay_time_ = 0;
};

} // namespace jazzlights

#endif // JAZZLIGHTS_UI_AUDIO_H
