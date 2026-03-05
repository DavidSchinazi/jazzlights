#include "jazzlights/ui/ui_audio.h"

#include "jazzlights/config.h"
#include "jazzlights/player.h"

#if JL_AUDIO_VISUALIZER

#if JL_IS_CONTROLLER(CORES3)

#include <M5Unified.h>

#include "driver/i2s.h"
#include "esp_dsp.h"
#include "esp_log.h"
#include "jazzlights/util/log.h"

namespace jazzlights {

// FFT constants
#define FFT_N 256
#define SAMPLE_RATE 16000

// Screen dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define BAR_WIDTH (SCREEN_WIDTH / (FFT_N / 2))

// Buffers for audio samples and FFT
static int16_t* audio_buffer = nullptr;
static float* fft_input = nullptr;
static float* fft_output = nullptr;
static float* fft_window = nullptr;

AudioVisualizerUi::AudioVisualizerUi(Player& player) : Esp32Ui(player) {}

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
  mic_cfg.magnification = 16;
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

void AudioVisualizerUi::RunLoop(Milliseconds /*currentTime*/) {
  size_t bytes_read = 0;
  if (M5.Mic.record(audio_buffer, FFT_N, SAMPLE_RATE)) {
    bytes_read = FFT_N * sizeof(int16_t);
    jll_info("Read %d bytes from microphone, first sample: %d", bytes_read, audio_buffer[0]);
    for (int i = 0; i < FFT_N; i++) {
      fft_input[i * 2] = (float)audio_buffer[i];
      fft_input[i * 2 + 1] = 0.0f;
    }

    // Apply window and perform FFT
    dsps_mul_f32(fft_input, fft_window, fft_input, FFT_N, 2, 1, 2);
    ESP_ERROR_CHECK(dsps_fft2r_fc32(fft_input, FFT_N));
    dsps_bit_rev_fc32(fft_input, FFT_N);

    // Convert to magnitude
    for (int i = 0; i < FFT_N / 2; i++) {
      float real = fft_input[i * 2];
      float imag = fft_input[i * 2 + 1];
      float power = real * real + imag * imag;
      fft_output[i] = 10 * log10f(power + 1e-10f);  // Add small epsilon to avoid log(0)
      jll_info("FFT magnitude [%d]: %f (r=%f, i=%f, p=%f)", i, fft_output[i], real, imag, power);
    }

    M5.Lcd.fillScreen(BLACK);

    for (int i = 2; i < FFT_N / 2; i++) {
      float magnitude = fft_output[i];
      int height = (int)(magnitude * 2);
      if (height > SCREEN_HEIGHT) height = SCREEN_HEIGHT;
      if (height < 0) height = 0;

      int x = i * BAR_WIDTH;
      M5.Lcd.fillRect(x, SCREEN_HEIGHT - height, BAR_WIDTH - 1, height, GREEN);
    }
  } else {
    jll_info("Could not read from I2S");
  }
  vTaskDelay(pdMS_TO_TICKS(10));
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
