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

  // Setup I2S for microphone
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 2,
      .dma_buf_len = FFT_N,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0,
  };
  i2s_pin_config_t pin_config = {
      .mck_io_num = GPIO_NUM_0,
      .bck_io_num = GPIO_NUM_34,
      .ws_io_num = GPIO_NUM_33,
      .data_out_num = GPIO_NUM_NC,
      .data_in_num = GPIO_NUM_14,
  };
  ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL));
  ESP_ERROR_CHECK(i2s_set_pin(I2S_NUM_0, &pin_config));
  ESP_ERROR_CHECK(i2s_set_clk(I2S_NUM_0, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO));
  jll_info("I2S microphone initialized");

  // Allocate memory
  audio_buffer = (int16_t*)malloc(FFT_N * sizeof(int16_t));
  fft_input = (float*)malloc(FFT_N * sizeof(float));
  fft_output = (float*)malloc(FFT_N * sizeof(float) * 2);
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
  ESP_ERROR_CHECK(i2s_read(I2S_NUM_0, audio_buffer, FFT_N * sizeof(int16_t), &bytes_read, pdMS_TO_TICKS(100)));

  if (bytes_read > 0) {
    jll_info("Read %d bytes from I2S", bytes_read);
    for (int i = 0; i < FFT_N; i++) { fft_input[i] = (float)audio_buffer[i]; }

    // Apply window and perform FFT
    dsps_mul_f32(fft_input, fft_window, fft_input, FFT_N, 1, 1, 1);
    ESP_ERROR_CHECK(dsps_fft2r_fc32((float*)fft_input, FFT_N));
    dsps_bit_rev_fc32(fft_input, FFT_N);

    // Convert to magnitude
    for (int i = 0; i < FFT_N / 2; i++) {
      float real = fft_input[i * 2];
      float imag = fft_input[i * 2 + 1];
      fft_output[i] = 10 * log10f(real * real + imag * imag);
    }
    jll_info("FFT magnitude [10]: %f", fft_output[10]);

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
