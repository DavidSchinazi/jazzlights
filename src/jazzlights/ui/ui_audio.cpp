#include "jazzlights/ui/ui_audio.h"

#ifdef ESP32
#if JL_AUDIO_VISUALIZER && (JL_IS_CONTROLLER(CORES3) || JL_IS_CONTROLLER(CORE2AWS) || JL_IS_CONTROLLER(M5STICK_C))

#include <Arduino.h>
#include <M5Unified.h>

#include <cinttypes>
#include <cstring>

#include "jazzlights/audio.h"
#include "jazzlights/palette.h"
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

#if JL_IS_CONTROLLER(M5STICK_C)
  M5.Display.setRotation(1);
#endif  // M5STICK_C

  M5.Display.setBrightness(128);
  M5.Display.fillScreen(BLACK);
  screen_width_ = M5.Display.width();
  screen_height_ = M5.Display.height();
  waveform_buffer_.assign(screen_width_, 0.0f);
  beat_buffer_.assign(screen_width_, false);
  jll_info("Audio visualizer UI setup complete w=%d h=%d", screen_width_, screen_height_);
}

void AudioVisualizerUi::FinalSetup() {}

void AudioVisualizerUi::RunLoop() {
  Milliseconds currentTime = timeMillis();
  M5.update();
  if ((M5.Touch.getCount() > 0 && M5.Touch.getDetail(0).wasPressed()) || M5.BtnB.wasPressed()) {
    auto detail = M5.Touch.getDetail(0);
    if (visualization_mode_ == VisualizationMode::kMenu) {
      if (detail.y >= 10 && detail.y <= 48 && detail.x >= 20 && detail.x <= 300) {
        visualization_mode_ = VisualizationMode::kSpectrum;
        jll_info("Switched to spectrum mode");
        M5.Display.fillScreen(BLACK);
      } else if (detail.y >= 53 && detail.y <= 91 && detail.x >= 20 && detail.x <= 300) {
        visualization_mode_ = VisualizationMode::kWaveform;
        jll_info("Switched to waveform mode");
        M5.Display.fillScreen(BLACK);
      } else if (detail.y >= 96 && detail.y <= 134 && detail.x >= 20 && detail.x <= 300) {
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
        jll_info("Toggled sound reactive to %s", mode_str);
        M5.Display.fillScreen(BLACK);
      } else if (detail.y >= 139 && detail.y <= 177 && detail.x >= 20 && detail.x <= 300) {
        visualization_mode_ = VisualizationMode::kBrightnessKeypad;
        keypad_value_ = 0;
        keypad_has_value_ = false;
        jll_info("Switched to brightness keypad");
        M5.Display.fillScreen(BLACK);
      } else if (detail.y >= 182 && detail.y <= 220 && detail.x >= 20 && detail.x <= 300) {
        visualization_mode_ = VisualizationMode::kPaletteMenu;
        jll_info("Switched to palette menu");
        M5.Display.fillScreen(BLACK);
      }
    } else if (visualization_mode_ == VisualizationMode::kBrightnessKeypad) {
      const int w = screen_width_ / 3;
      const int h = screen_height_ / 5;
      int col = detail.x / w;
      int row = detail.y / h;
      if (row == 0 && col == 0) {
        visualization_mode_ = VisualizationMode::kMenu;
        M5.Display.fillScreen(BLACK);
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
            jll_info("Set brightness to %" PRId32, keypad_value_);
          }
          visualization_mode_ = VisualizationMode::kMenu;
          M5.Display.fillScreen(BLACK);
        }
      }
    } else if (visualization_mode_ == VisualizationMode::kPaletteMenu) {
      const int w = screen_width_ / 3;
      const int h = screen_height_ / 3;
      int col = detail.x / w;
      int row = detail.y / h;
      if (row == 0 && col == 0) {
        visualization_mode_ = VisualizationMode::kMenu;
        M5.Display.fillScreen(BLACK);
      } else if (row == 0 && col == 1) {
        player_.stopForcePalette();
        visualization_mode_ = VisualizationMode::kMenu;
        M5.Display.fillScreen(BLACK);
      } else {
        int palette_idx = -1;
        if (row == 0 && col == 2)
          palette_idx = 3;  // Cloud
        else if (row == 1 && col == 0)
          palette_idx = 1;  // Lava
        else if (row == 1 && col == 1)
          palette_idx = 2;  // Ocean
        else if (row == 1 && col == 2)
          palette_idx = 5;  // Forest
        else if (row == 2 && col == 0)
          palette_idx = 6;  // Rainbow
        else if (row == 2 && col == 1)
          palette_idx = 4;  // Party
        else if (row == 2 && col == 2)
          palette_idx = 0;  // Heat

        if (palette_idx >= 0) {
          player_.forcePalette(static_cast<uint8_t>(palette_idx), currentTime);
          visualization_mode_ = VisualizationMode::kMenu;
          M5.Display.fillScreen(BLACK);
        }
      }
    } else {
      visualization_mode_ = VisualizationMode::kMenu;
      jll_info("Switched to menu mode");
      M5.Display.fillScreen(BLACK);
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
    waveform_index_ = (waveform_index_ + 1) % screen_width_;
    last_waveform_update_ += 12.5;
  }

  // Drawing
  M5.Display.startWrite();

  const bool no_audio_data = data.last_read_time < 0 || currentTime - data.last_read_time > 1000;
  if (no_audio_data != showing_no_audio_data_) {
    showing_no_audio_data_ = no_audio_data;
    jll_info("%s 'No Audio Data' mode", showing_no_audio_data_ ? "Entered" : "Exited");
    M5.Display.fillScreen(BLACK);
  }

  if (data.squelch != showing_squelch_) {
    showing_squelch_ = data.squelch;
    jll_info("%s 'Squelch' mode", showing_squelch_ ? "Entered" : "Exited");
    M5.Display.fillScreen(BLACK);
  }

  if (visualization_mode_ == VisualizationMode::kMenu) {
    M5.Display.setTextSize(2);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.drawRect(20, 10, 280, 38, WHITE);
    M5.Display.drawString("Spectrum Analyzer", screen_width_ / 2, 29);

    M5.Display.drawRect(20, 53, 280, 38, WHITE);
    M5.Display.drawString("Beat Detection", screen_width_ / 2, 72);

    M5.Display.drawRect(20, 96, 280, 38, WHITE);
    const char* mode_label = "UNKNOWN";
    switch (player_.sound_reactive_mode()) {
      case Player::SoundReactiveMode::kAuto: mode_label = "Auto"; break;
      case Player::SoundReactiveMode::kOn: mode_label = "On"; break;
      case Player::SoundReactiveMode::kOff: mode_label = "Off"; break;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "Sound Reactive: %s", mode_label);
    M5.Display.drawString(buf, screen_width_ / 2, 115);

    M5.Display.drawRect(20, 139, 280, 38, WHITE);
    snprintf(buf, sizeof(buf), "Brightness: %u", player_.brightness());
    M5.Display.drawString(buf, screen_width_ / 2, 158);

    M5.Display.drawRect(20, 182, 280, 38, WHITE);
    if (player_.paletteIsForced()) {
      snprintf(buf, sizeof(buf), "Palette: %s", OurColorPaletteName(player_.forcedPalette()).c_str());
    } else {
      snprintf(buf, sizeof(buf), "Palette: Default");
    }
    M5.Display.drawString(buf, screen_width_ / 2, 201);

    if (showing_no_audio_data_) {
      M5.Display.setTextColor(RED, BLACK);
      M5.Display.drawString("No Audio Data", screen_width_ / 2, 230);
    } else if (showing_squelch_) {
      M5.Display.setTextColor(ORANGE, BLACK);
      M5.Display.drawString("Squelch", screen_width_ / 2, 230);
    } else {
      M5.Display.fillRect(0, 225, screen_width_, 15, BLACK);
    }
  } else if (visualization_mode_ == VisualizationMode::kBrightnessKeypad) {
    const int w = screen_width_ / 3;
    const int h = screen_height_ / 5;
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.drawRect(0, 0, w, h, WHITE);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.drawString("Back", w / 2, h / 2);

    M5.Display.drawRect(w, 0, screen_width_ - w, h, WHITE);
    M5.Display.fillRect(w + 2, 2, screen_width_ - w - 4, h - 4, BLACK);
    char buf[64];
    if (keypad_has_value_) {
      snprintf(buf, sizeof(buf), "%" PRId32, keypad_value_);
    } else {
      snprintf(buf, sizeof(buf), "_ (curr %u)", player_.brightness());
    }
    M5.Display.drawString(buf, w + (screen_width_ - w) / 2, h / 2);

    for (int i = 1; i <= 9; i++) {
      int row = (i - 1) / 3 + 1;
      int col = (i - 1) % 3;
      M5.Display.drawRect(col * w, row * h, w, h, WHITE);
      snprintf(buf, sizeof(buf), "%d", i);
      M5.Display.drawString(buf, col * w + w / 2, row * h + h / 2);
    }
    M5.Display.drawRect(0, 4 * h, w, h, WHITE);
    M5.Display.drawString("Clear", w / 2, 4 * h + h / 2);
    M5.Display.drawRect(w, 4 * h, w, h, WHITE);
    M5.Display.drawString("0", w + w / 2, 4 * h + h / 2);
    M5.Display.drawRect(2 * w, 4 * h, w, h, WHITE);
    M5.Display.drawString("Confirm", 2 * w + w / 2, 4 * h + h / 2);
  } else if (visualization_mode_ == VisualizationMode::kPaletteMenu) {
    const int w = screen_width_ / 3;
    const int h = screen_height_ / 3;
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.setTextDatum(MC_DATUM);

    auto drawCell = [&](int row, int col, const char* label, bool selected) {
      M5.Display.drawRect(col * w, row * h, w, h, WHITE);
      M5.Display.fillRect(col * w + 2, row * h + 2, w - 4, h - 4, selected ? WHITE : BLACK);
      if (selected) {
        M5.Display.setTextColor(BLACK, WHITE);
      } else {
        M5.Display.setTextColor(WHITE, BLACK);
      }
      M5.Display.drawString(label, col * w + w / 2, row * h + h / 2);
    };

    drawCell(0, 0, "Back", false);
    drawCell(0, 1, "Default", !player_.paletteIsForced());
    drawCell(0, 2, "Cloud", player_.paletteIsForced() && player_.forcedPalette() == 3);
    drawCell(1, 0, "Lava", player_.paletteIsForced() && player_.forcedPalette() == 1);
    drawCell(1, 1, "Ocean", player_.paletteIsForced() && player_.forcedPalette() == 2);
    drawCell(1, 2, "Forest", player_.paletteIsForced() && player_.forcedPalette() == 5);
    drawCell(2, 0, "Rainbow",
             player_.paletteIsForced() && (player_.forcedPalette() == 6 || player_.forcedPalette() == 7));
    drawCell(2, 1, "Party", player_.paletteIsForced() && player_.forcedPalette() == 4);
    drawCell(2, 2, "Heat", player_.paletteIsForced() && player_.forcedPalette() == 0);
  } else if (showing_no_audio_data_) {
    M5.Display.setTextSize(2);
    M5.Display.setTextDatum(TC_DATUM);
    M5.Display.setTextColor(RED, BLACK);
    M5.Display.drawString("No Audio Data", screen_width_ / 2, 20);
    M5.Display.setTextSize(1);
  } else if (showing_squelch_) {
    M5.Display.setTextSize(2);
    M5.Display.setTextDatum(TC_DATUM);
    M5.Display.setTextColor(ORANGE, BLACK);
    M5.Display.drawString("Squelch", screen_width_ / 2, 20);
    M5.Display.setTextSize(1);
  } else {
    int bar_width = screen_width_ / Audio::kNumBands;
    if (visualization_mode_ == VisualizationMode::kSpectrum) {
      float max_db = data.agc_max;
      float min_db = max_db - 30.0f;  // Use a fixed 30dB dynamic range for the spectrum to keep it bright

      for (int i = 0; i < Audio::kNumBands; i++) {
        float mag = data.bands[i];
        int h = (int)((mag - min_db) * screen_height_ / (max_db - min_db));
        if (h > screen_height_) h = screen_height_;
        if (h < 0) h = 0;

        float pmag = data.peaks[i];
        int ph = (int)((pmag - min_db) * screen_height_ / (max_db - min_db));
        if (ph > screen_height_) ph = screen_height_;
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
        uint16_t color = M5.Display.color565(r, g, b);

        // Draw bar - clear background above bar
        if (h < screen_height_) { M5.Display.fillRect(x, 0, bar_width - 1, screen_height_ - h, BLACK); }
        // Draw the main bar
        M5.Display.fillRect(x, screen_height_ - h, bar_width - 1, h, color);

        // Draw peak indicator (single line or small rect)
        if (ph > 0) { M5.Display.drawFastHLine(x, screen_height_ - ph, bar_width - 1, WHITE); }
      }
    } else {
      float max_db = data.agc_max;
      float min_db = data.agc_min;
      // Waveform drawing logic
      for (int i = 0; i < screen_width_; i++) {
        int idx = (waveform_index_ - 1 - i + screen_width_) % screen_width_;
        float mag = waveform_buffer_[idx];
        bool is_beat = beat_buffer_[idx];
        int h = (int)((mag - min_db) * screen_height_ / (max_db - min_db));
        if (h > screen_height_) h = screen_height_;
        if (h < 0) h = 0;

        // Clear top, draw line
        if (is_beat) {
          M5.Display.drawFastVLine(i, 0, screen_height_, RED);
        } else {
          if (h < screen_height_) { M5.Display.drawFastVLine(i, 0, screen_height_ - h, BLACK); }
          if (h > 0) { M5.Display.drawFastVLine(i, screen_height_ - h, h, CYAN); }
        }
      }
    }
  }

  M5.Display.endWrite();
}

}  // namespace jazzlights

#endif  // JL_AUDIO_VISUALIZER && (CORES3 || CORE2AWS || M5STICK_C)
#endif  // ESP32
