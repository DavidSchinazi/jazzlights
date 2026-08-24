#include "jazzlights/ui/ui_audio.h"

#ifdef ESP32
#if JL_AUDIO_VISUALIZER && (JL_IS_CONTROLLER(CORES3) || JL_IS_CONTROLLER(CORE2AWS) || JL_IS_CONTROLLER(M5STICK_C))

#include <Arduino.h>
#include <M5Unified.h>

#include <cinttypes>
#include <cstring>

#include "jazzlights/audio/audio.h"
#include "jazzlights/effect/palette.h"
#include "jazzlights/render/player.h"
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
  screenWidth_ = M5.Display.width();
  screenHeight_ = M5.Display.height();
  waveformBuffer_.assign(screenWidth_, 0.0f);
  beatBuffer_.assign(screenWidth_, false);
  jll_info("Audio visualizer UI setup complete w=%d h=%d", screenWidth_, screenHeight_);
}

void AudioVisualizerUi::FinalSetup() {}

void AudioVisualizerUi::RunLoop() {
  Microseconds currentTime = TimeMicros();
  M5.update();
  if ((M5.Touch.getCount() > 0 && M5.Touch.getDetail(0).wasPressed()) || M5.BtnB.wasPressed()) {
    auto detail = M5.Touch.getDetail(0);
    if (visualizationMode_ == VisualizationMode::kMenu) {
      if (detail.y >= 10 && detail.y <= 48 && detail.x >= 20 && detail.x <= 300) {
        visualizationMode_ = VisualizationMode::kSpectrum;
        jll_info("Switched to spectrum mode");
        M5.Display.fillScreen(BLACK);
      } else if (detail.y >= 53 && detail.y <= 91 && detail.x >= 20 && detail.x <= 300) {
        visualizationMode_ = VisualizationMode::kWaveform;
        jll_info("Switched to waveform mode");
        M5.Display.fillScreen(BLACK);
      } else if (detail.y >= 96 && detail.y <= 134 && detail.x >= 20 && detail.x <= 300) {
        Player::SoundReactiveMode nextMode;
        switch (player_.soundReactiveMode()) {
          case Player::SoundReactiveMode::kAuto: nextMode = Player::SoundReactiveMode::kOn; break;
          case Player::SoundReactiveMode::kOn: nextMode = Player::SoundReactiveMode::kOff; break;
          case Player::SoundReactiveMode::kOff: nextMode = Player::SoundReactiveMode::kAuto; break;
        }
        player_.SetSoundReactiveMode(nextMode);
        const char* modeStr = "UNKNOWN";
        switch (nextMode) {
          case Player::SoundReactiveMode::kAuto: modeStr = "AUTO"; break;
          case Player::SoundReactiveMode::kOn: modeStr = "ON"; break;
          case Player::SoundReactiveMode::kOff: modeStr = "OFF"; break;
        }
        jll_info("Toggled sound reactive to %s", modeStr);
        M5.Display.fillScreen(BLACK);
      } else if (detail.y >= 139 && detail.y <= 177 && detail.x >= 20 && detail.x <= 300) {
        visualizationMode_ = VisualizationMode::kBrightnessKeypad;
        keypadValue_ = 0;
        keypad_has_value_ = false;
        jll_info("Switched to brightness keypad");
        M5.Display.fillScreen(BLACK);
      } else if (detail.y >= 182 && detail.y <= 220 && detail.x >= 20 && detail.x <= 300) {
        visualizationMode_ = VisualizationMode::kPaletteMenu;
        jll_info("Switched to palette menu");
        M5.Display.fillScreen(BLACK);
      }
    } else if (visualizationMode_ == VisualizationMode::kBrightnessKeypad) {
      const int w = screenWidth_ / 3;
      const int h = screenHeight_ / 5;
      int col = detail.x / w;
      int row = detail.y / h;
      if (row == 0 && col == 0) {
        visualizationMode_ = VisualizationMode::kMenu;
        M5.Display.fillScreen(BLACK);
      } else if (row >= 1 && row <= 3) {
        int val = (row - 1) * 3 + col + 1;
        if (keypadValue_ < 100) {
          keypadValue_ = keypadValue_ * 10 + val;
          keypad_has_value_ = true;
        }
      } else if (row == 4) {
        if (col == 0) {
          keypadValue_ = 0;
          keypad_has_value_ = false;
        } else if (col == 1) {
          if (keypadValue_ < 100) {
            keypadValue_ = keypadValue_ * 10;
            keypad_has_value_ = true;
          }
        } else if (col == 2) {
          if (keypad_has_value_) {
            if (keypadValue_ > 255) keypadValue_ = 255;
            player_.SetBrightness(keypadValue_);
            jll_info("Set brightness to %d", keypadValue_);
          }
          visualizationMode_ = VisualizationMode::kMenu;
          M5.Display.fillScreen(BLACK);
        }
      }
    } else if (visualizationMode_ == VisualizationMode::kPaletteMenu) {
      const int w = screenWidth_ / 3;
      const int h = screenHeight_ / 3;
      int col = detail.x / w;
      int row = detail.y / h;
      if (row == 0 && col == 0) {
        visualizationMode_ = VisualizationMode::kMenu;
        M5.Display.fillScreen(BLACK);
      } else if (row == 0 && col == 1) {
        player_.StopForcePalette();
        visualizationMode_ = VisualizationMode::kMenu;
        M5.Display.fillScreen(BLACK);
      } else {
        int paletteIdx = -1;
        if (row == 0 && col == 2)
          paletteIdx = 3;  // Cloud
        else if (row == 1 && col == 0)
          paletteIdx = 1;  // Lava
        else if (row == 1 && col == 1)
          paletteIdx = 2;  // Ocean
        else if (row == 1 && col == 2)
          paletteIdx = 5;  // Forest
        else if (row == 2 && col == 0)
          paletteIdx = 6;  // Rainbow
        else if (row == 2 && col == 1)
          paletteIdx = 4;  // Party
        else if (row == 2 && col == 2)
          paletteIdx = 0;  // Heat

        if (paletteIdx >= 0) {
          player_.ForcePalette(static_cast<uint8_t>(paletteIdx));
          visualizationMode_ = VisualizationMode::kMenu;
          M5.Display.fillScreen(BLACK);
        }
      }
    } else {
      visualizationMode_ = VisualizationMode::kMenu;
      jll_info("Switched to menu mode");
      M5.Display.fillScreen(BLACK);
    }
  }

  Audio::VisualizerData data;
  Audio::Get().GetVisualizerData(&data);

  float maxMag = 0;
  for (int i = 0; i < Audio::kNumBands; i++) {
    if (data.bands[i] > maxMag) maxMag = data.bands[i];
  }

  if (!lastWaveformUpdate_ || currentTime - *lastWaveformUpdate_ > 5 * kMicrosecondsPerSecond) {
    lastWaveformUpdate_ = currentTime;
  }
  static constexpr Microseconds kWaveformUpdatePeriod = 12500;  // 12.5ms.
  while (*lastWaveformUpdate_ + kWaveformUpdatePeriod <= currentTime) {
    waveformBuffer_[waveformIndex_] = maxMag;
    beatBuffer_[waveformIndex_] = data.beat;
    waveformIndex_ = (waveformIndex_ + 1) % screenWidth_;
    *lastWaveformUpdate_ += kWaveformUpdatePeriod;
  }

  // Drawing
  M5.Display.startWrite();

  const bool noAudioData = !data.lastReadTime || currentTime - *data.lastReadTime > kMicrosecondsPerSecond;
  if (noAudioData != showingNoAudioData_) {
    showingNoAudioData_ = noAudioData;
    jll_info("%s 'No Audio Data' mode", showingNoAudioData_ ? "Entered" : "Exited");
    M5.Display.fillScreen(BLACK);
  }

  if (data.squelch != showingSquelch_) {
    showingSquelch_ = data.squelch;
    jll_info("%s 'Squelch' mode", showingSquelch_ ? "Entered" : "Exited");
    M5.Display.fillScreen(BLACK);
  }

  if (visualizationMode_ == VisualizationMode::kMenu) {
    M5.Display.setTextSize(2);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.drawRect(20, 10, 280, 38, WHITE);
    M5.Display.drawString("Spectrum Analyzer", screenWidth_ / 2, 29);

    M5.Display.drawRect(20, 53, 280, 38, WHITE);
    M5.Display.drawString("Beat Detection", screenWidth_ / 2, 72);

    M5.Display.drawRect(20, 96, 280, 38, WHITE);
    const char* modeLabel = "UNKNOWN";
    switch (player_.soundReactiveMode()) {
      case Player::SoundReactiveMode::kAuto: modeLabel = "Auto"; break;
      case Player::SoundReactiveMode::kOn: modeLabel = "On"; break;
      case Player::SoundReactiveMode::kOff: modeLabel = "Off"; break;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "Sound Reactive: %s", modeLabel);
    M5.Display.drawString(buf, screenWidth_ / 2, 115);

    M5.Display.drawRect(20, 139, 280, 38, WHITE);
    snprintf(buf, sizeof(buf), "Brightness: %u", player_.brightness());
    M5.Display.drawString(buf, screenWidth_ / 2, 158);

    M5.Display.drawRect(20, 182, 280, 38, WHITE);
    snprintf(buf, sizeof(buf), "Palette: %s",
             player_.forcedPalette() ? OurColorPaletteName(*player_.forcedPalette()).c_str() : "Default");
    M5.Display.drawString(buf, screenWidth_ / 2, 201);

    if (showingNoAudioData_) {
      M5.Display.setTextColor(RED, BLACK);
      M5.Display.drawString("No Audio Data", screenWidth_ / 2, 230);
    } else if (showingSquelch_) {
      M5.Display.setTextColor(ORANGE, BLACK);
      M5.Display.drawString("Squelch", screenWidth_ / 2, 230);
    } else {
      M5.Display.fillRect(0, 225, screenWidth_, 15, BLACK);
    }
  } else if (visualizationMode_ == VisualizationMode::kBrightnessKeypad) {
    const int w = screenWidth_ / 3;
    const int h = screenHeight_ / 5;
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.drawRect(0, 0, w, h, WHITE);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.drawString("Back", w / 2, h / 2);

    M5.Display.drawRect(w, 0, screenWidth_ - w, h, WHITE);
    M5.Display.fillRect(w + 2, 2, screenWidth_ - w - 4, h - 4, BLACK);
    char buf[64];
    if (keypad_has_value_) {
      snprintf(buf, sizeof(buf), "%d", keypadValue_);
    } else {
      snprintf(buf, sizeof(buf), "_ (curr %u)", player_.brightness());
    }
    M5.Display.drawString(buf, w + (screenWidth_ - w) / 2, h / 2);

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
  } else if (visualizationMode_ == VisualizationMode::kPaletteMenu) {
    const int w = screenWidth_ / 3;
    const int h = screenHeight_ / 3;
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

    uint8_t forcedPalette = player_.forcedPalette().value_or(255);
    drawCell(0, 0, "Back", false);
    drawCell(0, 1, "Default", !player_.forcedPalette());
    drawCell(0, 2, "Cloud", forcedPalette == 3);
    drawCell(1, 0, "Lava", forcedPalette == 1);
    drawCell(1, 1, "Ocean", forcedPalette == 2);
    drawCell(1, 2, "Forest", forcedPalette == 5);
    drawCell(2, 0, "Rainbow", forcedPalette == 6 || forcedPalette == 7);
    drawCell(2, 1, "Party", forcedPalette == 4);
    drawCell(2, 2, "Heat", forcedPalette == 0);
  } else if (showingNoAudioData_) {
    M5.Display.setTextSize(2);
    M5.Display.setTextDatum(TC_DATUM);
    M5.Display.setTextColor(RED, BLACK);
    M5.Display.drawString("No Audio Data", screenWidth_ / 2, 20);
    M5.Display.setTextSize(1);
  } else if (showingSquelch_) {
    M5.Display.setTextSize(2);
    M5.Display.setTextDatum(TC_DATUM);
    M5.Display.setTextColor(ORANGE, BLACK);
    M5.Display.drawString("Squelch", screenWidth_ / 2, 20);
    M5.Display.setTextSize(1);
  } else {
    int barWidth = screenWidth_ / Audio::kNumBands;
    if (visualizationMode_ == VisualizationMode::kSpectrum) {
      float maxDb = data.agcMax;
      float minDb = maxDb - 30.0f;  // Use a fixed 30dB dynamic range for the spectrum to keep it bright

      for (int i = 0; i < Audio::kNumBands; i++) {
        float mag = data.bands[i];
        int h = (int)((mag - minDb) * screenHeight_ / (maxDb - minDb));
        if (h > screenHeight_) h = screenHeight_;
        if (h < 0) h = 0;

        float pmag = data.peaks[i];
        int ph = (int)((pmag - minDb) * screenHeight_ / (maxDb - minDb));
        if (ph > screenHeight_) ph = screenHeight_;
        if (ph < 0) ph = 0;

        int x = i * barWidth;

        // Calculate color based on frequency (Rainbow)
        float hue = (float)i / Audio::kNumBands * 255.0f;
        // Simple HSV to RGB mapping (V=1, S=1)
        uint8_t r, g, b;
        float sector = hue / 42.5f;  // 6 sectors
        int iSector = (int)sector;
        float fSector = sector - iSector;
        uint8_t p = 0;
        uint8_t q = (uint8_t)(255 * (1.0f - fSector));
        uint8_t t = (uint8_t)(255 * fSector);
        switch (iSector) {
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
        if (h < screenHeight_) { M5.Display.fillRect(x, 0, barWidth - 1, screenHeight_ - h, BLACK); }
        // Draw the main bar
        M5.Display.fillRect(x, screenHeight_ - h, barWidth - 1, h, color);

        // Draw peak indicator (single line or small rect)
        if (ph > 0) { M5.Display.drawFastHLine(x, screenHeight_ - ph, barWidth - 1, WHITE); }
      }
    } else {
      float maxDb = data.agcMax;
      float minDb = data.agcMin;
      // Waveform drawing logic
      for (int i = 0; i < screenWidth_; i++) {
        int idx = (waveformIndex_ - 1 - i + screenWidth_) % screenWidth_;
        float mag = waveformBuffer_[idx];
        bool isBeat = beatBuffer_[idx];
        int h = (int)((mag - minDb) * screenHeight_ / (maxDb - minDb));
        if (h > screenHeight_) h = screenHeight_;
        if (h < 0) h = 0;

        // Clear top, draw line
        if (isBeat) {
          M5.Display.drawFastVLine(i, 0, screenHeight_, RED);
        } else {
          if (h < screenHeight_) { M5.Display.drawFastVLine(i, 0, screenHeight_ - h, BLACK); }
          if (h > 0) { M5.Display.drawFastVLine(i, screenHeight_ - h, h, CYAN); }
        }
      }
    }
  }

  M5.Display.endWrite();
}

}  // namespace jazzlights

#endif  // JL_AUDIO_VISUALIZER && (CORES3 || CORE2AWS || M5STICK_C)
#endif  // ESP32
