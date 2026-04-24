#include "jazzlights/ui/ui_audio.h"

#ifdef ESP32
#if JL_AUDIO_VISUALIZER && (JL_IS_CONTROLLER(CORES3) || JL_IS_CONTROLLER(CORE2AWS))

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
  // Initialize M5 device
  auto cfg = M5.config();
  cfg.serial_baudrate = 0;
  cfg.internal_spk = false;
  cfg.internal_mic = false;
  M5.begin(cfg);
  jll_info("M5 device initialized");

  M5.Display.setBrightness(128);
  M5.Lcd.fillScreen(BLACK);

  jll_info("Audio visualizer UI setup complete");
}

void AudioVisualizerUi::FinalSetup() {}

void AudioVisualizerUi::RunLoop(Milliseconds currentTime) {
  M5.update();
  if (M5.Touch.getCount() > 0 && M5.Touch.getDetail(0).wasPressed()) {
    auto detail = M5.Touch.getDetail(0);
    if (visualization_mode_ == VisualizationMode::kMenu) {
      if (detail.y >= 40 && detail.y <= 100 && detail.x >= 20 && detail.x <= 300) {
        visualization_mode_ = VisualizationMode::kSpectrum;
        jll_info("%u Switched to spectrum mode", currentTime);
        M5.Lcd.fillScreen(BLACK);
      } else if (detail.y >= 120 && detail.y <= 180 && detail.x >= 20 && detail.x <= 300) {
        visualization_mode_ = VisualizationMode::kWaveform;
        jll_info("%u Switched to waveform mode", currentTime);
        M5.Lcd.fillScreen(BLACK);
      }
    } else {
      visualization_mode_ = VisualizationMode::kMenu;
      jll_info("%u Switched to menu mode", currentTime);
      M5.Lcd.fillScreen(BLACK);
    }
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

  const bool no_audio_data = data.last_read_time < 0 || currentTime - data.last_read_time > 1000;
  if (no_audio_data != showing_no_audio_data_) {
    showing_no_audio_data_ = no_audio_data;
    jll_info("%u %s 'No Audio Data' mode", currentTime, showing_no_audio_data_ ? "Entered" : "Exited");
    M5.Lcd.fillScreen(BLACK);
  }

  if (data.squelch != showing_squelch_) {
    showing_squelch_ = data.squelch;
    jll_info("%u %s 'Squelch' mode", currentTime, showing_squelch_ ? "Entered" : "Exited");
    M5.Lcd.fillScreen(BLACK);
  }

  if (visualization_mode_ == VisualizationMode::kMenu) {
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextDatum(MC_DATUM);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.drawRect(20, 40, 280, 60, WHITE);
    M5.Lcd.drawString("Spectrum Analyzer", kScreenWidth / 2, 70);

    M5.Lcd.drawRect(20, 120, 280, 60, WHITE);
    M5.Lcd.drawString("Beat Detection", kScreenWidth / 2, 150);

    if (showing_no_audio_data_) {
      M5.Lcd.setTextColor(RED, BLACK);
      M5.Lcd.drawString("No Audio Data", kScreenWidth / 2, 210);
    } else if (showing_squelch_) {
      M5.Lcd.setTextColor(ORANGE, BLACK);
      M5.Lcd.drawString("Squelch", kScreenWidth / 2, 210);
    } else {
      M5.Lcd.fillRect(0, 190, kScreenWidth, 50, BLACK);
    }
  } else if (showing_no_audio_data_) {
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextDatum(TC_DATUM);
    M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.drawString("No Audio Data", kScreenWidth / 2, 20);
    M5.Lcd.setTextSize(1);
  } else if (showing_squelch_) {
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextDatum(TC_DATUM);
    M5.Lcd.setTextColor(ORANGE, BLACK);
    M5.Lcd.drawString("Squelch", kScreenWidth / 2, 20);
    M5.Lcd.setTextSize(1);
  } else {
    int bar_width = kScreenWidth / Audio::kNumBands;
    if (visualization_mode_ == VisualizationMode::kSpectrum) {
      float max_db = data.agc_max;
      float min_db = max_db - 30.0f;  // Use a fixed 30dB dynamic range for the spectrum to keep it bright

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
  }

  M5.Lcd.endWrite();
}

}  // namespace jazzlights

#endif  // JL_AUDIO_VISUALIZER && (JL_IS_CONTROLLER(CORES3) || JL_IS_CONTROLLER(CORE2AWS))
#endif  // ESP32
