#include "jazzlights/ui/ui_audio.h"

#include "jazzlights/config.h"
#include "jazzlights/player.h"

#if JL_AUDIO_VISUALIZER

#if JL_IS_CONTROLLER(CORES3)

#include <M5Unified.h>
#include <esp_dsp.h>
#include <esp_log.h>

#include "jazzlights/util/log.h"

namespace jazzlights {

// FFT constants
#define FFT_N 256
#define SAMPLE_RATE 16000

// Screen dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Buffers for audio samples and FFT
static int16_t* audio_buffer = nullptr;
static float* fft_input = nullptr;
static float* fft_output = nullptr;
static float* fft_window = nullptr;

AudioVisualizerUi::AudioVisualizerUi(Player& player) : Esp32Ui(player) {
  for (int i = 0; i < kNumBands; i++) {
    band_magnitudes_[i] = 0.0f;
    peak_magnitudes_[i] = 0.0f;
  }
}

void AudioVisualizerUi::InitialSetup() {
  jll_info("Starting audio visualizer setup...");
  // Initialize M5CoreS3
  auto cfg = M5.config();
  M5.begin(cfg);
  jll_info("M5CoreS3 initialized");

  // Setup microphone using M5Unified
  auto mic_cfg = M5.Mic.config();
  mic_cfg.sample_rate = SAMPLE_RATE;
  mic_cfg.stereo = false;
  mic_cfg.magnification = 8;
  mic_cfg.noise_filter_level = 2;
  M5.Mic.config(mic_cfg);
  M5.Mic.begin();
  jll_info("M5 microphone initialized");

  // Allocate memory
  audio_buffer = (int16_t*)malloc(FFT_N * sizeof(int16_t));
  fft_input = (float*)malloc(FFT_N * 2 * sizeof(float));
  fft_output = (float*)malloc(FFT_N * sizeof(float));
  fft_window = (float*)malloc(FFT_N * sizeof(float));

  // Initialize FFT
  ESP_ERROR_CHECK(dsps_fft2r_init_fc32(NULL, FFT_N));
  dsps_wind_hann_f32(fft_window, FFT_N);
  jll_info("FFT initialized");

  M5.Lcd.fillScreen(BLACK);
  jll_info("Audio visualizer setup complete");
}

void AudioVisualizerUi::FinalSetup() {}

void AudioVisualizerUi::RunLoop(Milliseconds currentTime) {
  if (M5.Mic.record(audio_buffer, FFT_N, SAMPLE_RATE)) {
    for (int i = 0; i < FFT_N; i++) {
      fft_input[i * 2] = (float)audio_buffer[i];
      fft_input[i * 2 + 1] = 0.0f;
    }

    // Apply window and perform FFT
    dsps_mul_f32(fft_input, fft_window, fft_input, FFT_N, 2, 1, 2);
    ESP_ERROR_CHECK(dsps_fft2r_fc32(fft_input, FFT_N));
    dsps_bit_rev_fc32(fft_input, FFT_N);

    // Convert to magnitude (dB)
    for (int i = 0; i < FFT_N / 2; i++) {
      float real = fft_input[i * 2];
      float imag = fft_input[i * 2 + 1];
      float power = real * real + imag * imag;
      fft_output[i] = 10 * log10f(power + 1.0f);
    }

    // Map FFT bins to bands (logarithmic scaling)
    // 128 bins total. We'll skip bin 0 (DC).
    // Frequencies: 62.5Hz to 8000Hz.
    float min_freq = 62.5f;
    float max_freq = 8000.0f;
    float log_min = log2f(min_freq);
    float log_max = log2f(max_freq);
    float log_step = (log_max - log_min) / kNumBands;

    float new_bands[kNumBands];
    for (int i = 0; i < kNumBands; i++) {
      float start_freq = powf(2.0f, log_min + i * log_step);
      float end_freq = powf(2.0f, log_min + (i + 1) * log_step);

      int start_bin = (int)(start_freq / (SAMPLE_RATE / FFT_N));
      int end_bin = (int)(end_freq / (SAMPLE_RATE / FFT_N));
      if (end_bin <= start_bin) end_bin = start_bin + 1;
      if (start_bin < 1) start_bin = 1;
      if (end_bin > FFT_N / 2) end_bin = FFT_N / 2;

      float sum = 0;
      int count = 0;
      for (int b = start_bin; b < end_bin; b++) {
        sum += fft_output[b];
        count++;
      }
      new_bands[i] = (count > 0) ? (sum / count) : 0;
    }

    // Smoothing and Peak Decay
    float smoothing = 0.7f;
    float peak_decay = 0.5f;  // dB per frame
    if (last_peak_decay_time_ == 0) last_peak_decay_time_ = currentTime;
    float elapsed_s = (currentTime - last_peak_decay_time_) / 1000.0f;
    last_peak_decay_time_ = currentTime;

    for (int i = 0; i < kNumBands; i++) {
      band_magnitudes_[i] = band_magnitudes_[i] * smoothing + new_bands[i] * (1.0f - smoothing);
      if (new_bands[i] > peak_magnitudes_[i]) {
        peak_magnitudes_[i] = new_bands[i];
      } else {
        peak_magnitudes_[i] -= peak_decay;
        if (peak_magnitudes_[i] < 0) peak_magnitudes_[i] = 0;
      }
    }

    // Drawing
    M5.Lcd.startWrite();
    int bar_width = SCREEN_WIDTH / kNumBands;
    float min_db = 50.0f;  // Noise floor
    float max_db = 90.0f;  // Adjust based on sensitivity

    for (int i = 0; i < kNumBands; i++) {
      float mag = band_magnitudes_[i];
      int h = (int)((mag - min_db) * SCREEN_HEIGHT / (max_db - min_db));
      if (h > SCREEN_HEIGHT) h = SCREEN_HEIGHT;
      if (h < 0) h = 0;

      float pmag = peak_magnitudes_[i];
      int ph = (int)((pmag - min_db) * SCREEN_HEIGHT / (max_db - min_db));
      if (ph > SCREEN_HEIGHT) ph = SCREEN_HEIGHT;
      if (ph < 0) ph = 0;

      int x = i * bar_width;

      // Calculate color based on frequency (Rainbow)
      float hue = (float)i / kNumBands * 255.0f;
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
      if (h < SCREEN_HEIGHT) { M5.Lcd.fillRect(x, 0, bar_width - 1, SCREEN_HEIGHT - h, BLACK); }
      // Draw the main bar
      M5.Lcd.fillRect(x, SCREEN_HEIGHT - h, bar_width - 1, h, color);

      // Draw peak indicator (single line or small rect)
      if (ph > 0) { M5.Lcd.drawFastHLine(x, SCREEN_HEIGHT - ph, bar_width - 1, WHITE); }
    }
    M5.Lcd.endWrite();
  }
}

}  // namespace jazzlights

#else  // JL_IS_CONTROLLER(CORES3)

namespace jazzlights {

// Non-functional implementation for other controllers
AudioVisualizerUi::AudioVisualizerUi(Player& player) : Esp32Ui(player) {}
void AudioVisualizerUi::InitialSetup() {}
void AudioVisualizerUi::FinalSetup() {}
void AudioVisualizerUi::RunLoop(Milliseconds /*currentTime*/) {}

}  // namespace jazzlights

#endif  // JL_IS_CONTROLLER(CORES3)

#else  // JL_AUDIO_VISUALIZER

namespace jazzlights {

// Non-functional implementation when audio visualizer is disabled
AudioVisualizerUi::AudioVisualizerUi(Player& player) : Esp32Ui(player) {}
void AudioVisualizerUi::InitialSetup() {}
void AudioVisualizerUi::FinalSetup() {}
void AudioVisualizerUi::RunLoop(Milliseconds /*currentTime*/) {}

}  // namespace jazzlights

#endif  // JL_AUDIO_VISUALIZER
