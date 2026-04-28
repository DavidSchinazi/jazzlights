#include "jazzlights/ui/ui_audio.h"

#ifdef ESP32
#if JL_AUDIO_VISUALIZER && (JL_IS_CONTROLLER(CORES3) || JL_IS_CONTROLLER(CORE2AWS))

#include <Arduino.h>
#include <M5Unified.h>

#include <cinttypes>
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
      if (detail.y >= 10 && detail.y <= 55 && detail.x >= 20 && detail.x <= 300) {
        visualization_mode_ = VisualizationMode::kSpectrum;
        jll_info("%u Switched to spectrum mode", currentTime);
        M5.Lcd.fillScreen(BLACK);
      } else if (detail.y >= 65 && detail.y <= 110 && detail.x >= 20 && detail.x <= 300) {
        visualization_mode_ = VisualizationMode::kWaveform;
        jll_info("%u Switched to waveform mode", currentTime);
        M5.Lcd.fillScreen(BLACK);
      } else if (detail.y >= 120 && detail.y <= 165 && detail.x >= 20 && detail.x <= 300) {
        Player::SoundReactiveMode next_mode;
        switch (player_.sound_reactive_mode()) {
          case Player::SoundReactiveMode::kAuto: next_mode = Player::SoundReactiveMode::kOn; break;
          case Player::SoundReactiveMode::kOn: next_mode = Player::SoundReactiveMode::kOff; break;
          case Player::SoundReactiveMode::kOff: next_mode = Player::SoundReactiveMode::kAuto; break;
        }
        player_.set_sound_reactive_mode(next_mode);
        const char* mode_str = "UNKNOWN";
        switch (next_mode) {
          case Player::SoundReactiveMode::kAuto: mode_str = "AUTO"; break;
          case Player::SoundReactiveMode::kOn: mode_str = "ON"; break;
          case Player::SoundReactiveMode::kOff: mode_str = "OFF"; break;
        }
        jll_info("%u Toggled sound reactive to %s", currentTime, mode_str);
        M5.Lcd.fillScreen(BLACK);
      } else if (detail.y >= 175 && detail.y <= 220 && detail.x >= 20 && detail.x <= 300) {
        visualization_mode_ = VisualizationMode::kBrightnessKeypad;
        keypad_value_ = 0;
        keypad_has_value_ = false;
        jll_info("%u Switched to brightness keypad", currentTime);
        M5.Lcd.fillScreen(BLACK);
      }
    } else if (visualization_mode_ == VisualizationMode::kBrightnessKeypad) {
      const int w = kScreenWidth / 3;
      const int h = kScreenHeight / 5;
      int col = detail.x / w;
      int row = detail.y / h;
      if (row == 0 && col == 0) {
        visualization_mode_ = VisualizationMode::kMenu;
        M5.Lcd.fillScreen(BLACK);
      } else if (row >= 1 && row <= 3) {
        int val = (row - 1) * 3 + col + 1;
        if (keypad_value_ < 100) {
          keypad_value_ = keypad_value_ * 10 + val;
          keypad_has_value_ = true;
        }
      } else if (row == 4) {
        if (col == 0) {
          keypad_value_ = 0;
          keypad_has_value_ = false;
        } else if (col == 1) {
          if (keypad_value_ < 100) {
            keypad_value_ = keypad_value_ * 10;
            keypad_has_value_ = true;
          }
        } else if (col == 2) {
          if (keypad_has_value_) {
            if (keypad_value_ > 255) keypad_value_ = 255;
            player_.set_brightness(keypad_value_);
            jll_info("%u Set brightness to %" PRId32, currentTime, keypad_value_);
          }
          visualization_mode_ = VisualizationMode::kMenu;
          M5.Lcd.fillScreen(BLACK);
        }
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
    M5.Lcd.drawRect(20, 10, 280, 45, WHITE);
    M5.Lcd.drawString("Spectrum Analyzer", kScreenWidth / 2, 32);

    M5.Lcd.drawRect(20, 65, 280, 45, WHITE);
    M5.Lcd.drawString("Beat Detection", kScreenWidth / 2, 87);

    M5.Lcd.drawRect(20, 120, 280, 45, WHITE);
    const char* mode_label = "UNKNOWN";
    switch (player_.sound_reactive_mode()) {
      case Player::SoundReactiveMode::kAuto: mode_label = "Auto"; break;
      case Player::SoundReactiveMode::kOn: mode_label = "On"; break;
      case Player::SoundReactiveMode::kOff: mode_label = "Off"; break;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "Sound Reactive: %s", mode_label);
    M5.Lcd.drawString(buf, kScreenWidth / 2, 142);

    M5.Lcd.drawRect(20, 175, 280, 45, WHITE);
    snprintf(buf, sizeof(buf), "Brightness: %u", player_.brightness());
    M5.Lcd.drawString(buf, kScreenWidth / 2, 197);

    if (showing_no_audio_data_) {
      M5.Lcd.setTextColor(RED, BLACK);
      M5.Lcd.drawString("No Audio Data", kScreenWidth / 2, 230);
    } else if (showing_squelch_) {
      M5.Lcd.setTextColor(ORANGE, BLACK);
      M5.Lcd.drawString("Squelch", kScreenWidth / 2, 230);
    } else {
      M5.Lcd.fillRect(0, 225, kScreenWidth, 15, BLACK);
    }
  } else if (visualization_mode_ == VisualizationMode::kBrightnessKeypad) {
    const int w = kScreenWidth / 3;
    const int h = kScreenHeight / 5;
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.drawRect(0, 0, w, h, WHITE);
    M5.Lcd.setTextDatum(MC_DATUM);
    M5.Lcd.drawString("Back", w / 2, h / 2);

    M5.Lcd.drawRect(w, 0, kScreenWidth - w, h, WHITE);
    char buf[64];
    if (keypad_has_value_) {
      snprintf(buf, sizeof(buf), "%" PRId32, keypad_value_);
    } else {
      snprintf(buf, sizeof(buf), "_ (curr %u)", player_.brightness());
    }
    M5.Lcd.drawString(buf, w + (kScreenWidth - w) / 2, h / 2);

    for (int i = 1; i <= 9; i++) {
      int row = (i - 1) / 3 + 1;
      int col = (i - 1) % 3;
      M5.Lcd.drawRect(col * w, row * h, w, h, WHITE);
      snprintf(buf, sizeof(buf), "%d", i);
      M5.Lcd.drawString(buf, col * w + w / 2, row * h + h / 2);
    }
    M5.Lcd.drawRect(0, 4 * h, w, h, WHITE);
    M5.Lcd.drawString("Clear", w / 2, 4 * h + h / 2);
    M5.Lcd.drawRect(w, 4 * h, w, h, WHITE);
    M5.Lcd.drawString("0", w + w / 2, 4 * h + h / 2);
    M5.Lcd.drawRect(2 * w, 4 * h, w, h, WHITE);
    M5.Lcd.drawString("Confirm", 2 * w + w / 2, 4 * h + h / 2);
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
