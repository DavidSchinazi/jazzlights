#include "jazzlights/ui/ui_audio.h"

#if JL_AUDIO_VISUALIZER && JL_IS_CONTROLLER(CORES3)

#include <Arduino.h>
#include <M5Unified.h>

#include <cstring>

#include "jazzlights/audio.h"
#include "jazzlights/player.h"
#include "jazzlights/util/log.h"

namespace jazzlights {

AudioVisualizerUi::AudioVisualizerUi(Player& player) : Esp32Ui(player) {}

void AudioVisualizerUi::InitialSetup() {
  jll_info("Starting audio visualizer UI setup...");
  // Initialize M5CoreS3
  auto cfg = M5.config();
  M5.begin(cfg);
  jll_info("M5CoreS3 initialized");

  Audio::Get().Initialize();

  M5.Lcd.fillScreen(BLACK);

  jll_info("Audio visualizer UI setup complete");
}

void AudioVisualizerUi::FinalSetup() {}

void AudioVisualizerUi::RunLoop(Milliseconds currentTime) {
  M5.update();
  if (M5.Touch.getCount() > 0 && M5.Touch.getDetail(0).wasPressed()) {
    visualization_mode_ = (visualization_mode_ == VisualizationMode::kSpectrum) ? VisualizationMode::kWaveform
                                                                                : VisualizationMode::kSpectrum;
    M5.Lcd.fillScreen(BLACK);
  }

  Audio::VisualizerData data;
  Audio::Get().GetVisualizerData(&data);

  float max_mag = 0;
  for (int i = 0; i < Audio::kNumBands; i++) {
    if (data.bands[i] > max_mag) max_mag = data.bands[i];
  }

  if (last_waveform_update_ == 0 || currentTime - last_waveform_update_ > 5000) { last_waveform_update_ = currentTime; }
  while (last_waveform_update_ + 12.5 <= (double)currentTime) {
    waveform_buffer_[waveform_index_] = max_mag;
    beat_buffer_[waveform_index_] = data.beat;
    waveform_index_ = (waveform_index_ + 1) % kScreenWidth;
    last_waveform_update_ += 12.5;
  }

  // Drawing
  M5.Lcd.startWrite();
  int bar_width = kScreenWidth / Audio::kNumBands;

  if (visualization_mode_ == VisualizationMode::kSpectrum) {
    float max_db = data.agc_max;
    float min_db = max_db - 30.0f;  // Use a fixed 30dB dynamic range for the spectrum to keep it bright

    // Clear previous beat border
    M5.Lcd.drawRect(0, 0, kScreenWidth, kScreenHeight, BLACK);

    for (int i = 0; i < Audio::kNumBands; i++) {
      float mag = data.bands[i];
      int h = (int)((mag - min_db) * kScreenHeight / (max_db - min_db));
      if (h > kScreenHeight) h = kScreenHeight;
      if (h < 0) h = 0;

      float pmag = data.peaks[i];
      int ph = (int)((pmag - min_db) * kScreenHeight / (max_db - min_db));
      if (ph > kScreenHeight) ph = kScreenHeight;
      if (ph < 0) ph = 0;

      int x = i * bar_width;

      // Calculate color based on frequency (Rainbow)
      float hue = (float)i / Audio::kNumBands * 255.0f;
      // Simple HSV to RGB mapping (V=1, S=1)
      uint8_t r, g, b;
      float sector = hue / 42.5f;  // 6 sectors
      int i_sector = (int)sector;
      float f_sector = sector - i_sector;
      uint8_t p = 0;
      uint8_t q = (uint8_t)(255 * (1.0f - f_sector));
      uint8_t t = (uint8_t)(255 * f_sector);
      switch (i_sector) {
        case 0:
          r = 255;
          g = t;
          b = p;
          break;
        case 1:
          r = q;
          g = 255;
          b = p;
          break;
        case 2:
          r = p;
          g = 255;
          b = t;
          break;
        case 3:
          r = p;
          g = q;
          b = 255;
          break;
        case 4:
          r = t;
          g = p;
          b = 255;
          break;
        default:
          r = 255;
          g = p;
          b = q;
          break;
      }
      uint16_t color = M5.Lcd.color565(r, g, b);

      // Draw bar - clear background above bar
      if (h < kScreenHeight) { M5.Lcd.fillRect(x, 0, bar_width - 1, kScreenHeight - h, BLACK); }
      // Draw the main bar
      M5.Lcd.fillRect(x, kScreenHeight - h, bar_width - 1, h, color);

      // Draw peak indicator (single line or small rect)
      if (ph > 0) { M5.Lcd.drawFastHLine(x, kScreenHeight - ph, bar_width - 1, WHITE); }
    }
    // If beat detected, draw red border
    if (data.beat) { M5.Lcd.drawRect(0, 0, kScreenWidth, kScreenHeight, RED); }
  } else {
    float max_db = data.agc_max;
    float min_db = data.agc_min;
    // Waveform drawing logic
    for (int i = 0; i < kScreenWidth; i++) {
      int idx = (waveform_index_ - 1 - i + kScreenWidth) % kScreenWidth;
      float mag = waveform_buffer_[idx];
      bool is_beat = beat_buffer_[idx];
      int h = (int)((mag - min_db) * kScreenHeight / (max_db - min_db));
      if (h > kScreenHeight) h = kScreenHeight;
      if (h < 0) h = 0;

      // Clear top, draw line
      if (is_beat) {
        M5.Lcd.drawFastVLine(i, 0, kScreenHeight, RED);
      } else {
        if (h < kScreenHeight) { M5.Lcd.drawFastVLine(i, 0, kScreenHeight - h, BLACK); }
        if (h > 0) { M5.Lcd.drawFastVLine(i, kScreenHeight - h, h, CYAN); }
      }
    }
  }
  M5.Lcd.endWrite();
}

}  // namespace jazzlights

#endif  // JL_AUDIO_VISUALIZER && JL_IS_CONTROLLER(CORES3)
