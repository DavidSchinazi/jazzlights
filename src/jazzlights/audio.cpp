#include "jazzlights/audio.h"

#if JL_AUDIO_VISUALIZER

#include <M5Unified.h>
#include <driver/i2s_pdm.h>
#include <driver/i2s_std.h>
#include <esp_dsp.h>
#include <esp_log.h>

#include <cmath>
#include <cstring>

#include "jazzlights/util/log.h"

namespace jazzlights {

namespace {
static constexpr int kFFTSize = 256;
static constexpr uint32_t kSampleRate = 16000;
}  // namespace

#define JL_CORES3_USE_INTERNAL_MICROPHONE 0

#if (JL_IS_CONTROLLER(CORES3) && !JL_CORES3_USE_INTERNAL_MICROPHONE) || JL_IS_CONTROLLER(CORE2AWS)
static constexpr i2s_port_t kI2sPort = I2S_NUM_1;
#else
static constexpr i2s_port_t kI2sPort = I2S_NUM_0;
#endif

Audio& Audio::Get() {
  static Audio instance;
  return instance;
}

void Audio::Initialize() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(kI2sPort, I2S_ROLE_MASTER);
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, nullptr, &rx_handle_));

#if JL_IS_CONTROLLER(CORES3) || JL_IS_CONTROLLER(CORE2AWS)
  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(kSampleRate),
#if JL_IS_CONTROLLER(CORES3) && JL_CORES3_USE_INTERNAL_MICROPHONE
      .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
#else
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
#endif
      .gpio_cfg =
          {
#if JL_IS_CONTROLLER(CORES3) && JL_CORES3_USE_INTERNAL_MICROPHONE
                     // Internal microphone on CoreS3-SE.
              .mclk = GPIO_NUM_0,
                     .bclk = GPIO_NUM_34,
                     .ws = GPIO_NUM_33,
                     .dout = I2S_GPIO_UNUSED,
                     .din = GPIO_NUM_14,
#else   // External I2S microphone using PCM1808
              .mclk = static_cast<gpio_num_t>(kPinE1_3),  // Master Clock on the ESP32, Slave Clock on the PCM1808
              .bclk = static_cast<gpio_num_t>(kPinC1),    // Bit Clock / Serial Clock
              .ws = static_cast<gpio_num_t>(kPinE2_2),    // LRC (Word Select == Left Right CLock)
              .dout = I2S_GPIO_UNUSED,
              .din = static_cast<gpio_num_t>(kPinC2),  // OUT
#endif  // JL_CORES3_USE_INTERNAL_MICROPHONE
                     .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
                     },
  };
#if (JL_IS_CONTROLLER(CORES3) && !JL_CORES3_USE_INTERNAL_MICROPHONE) || JL_IS_CONTROLLER(CORE2AWS)
  std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;
#endif

  ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg));
#if JL_IS_CONTROLLER(CORES3) && JL_CORES3_USE_INTERNAL_MICROPHONE
  auto mic_cfg = M5.Mic.config();
  mic_cfg.pin_data_in = -1;  // Prevent M5Unified from initializing I2S and starting a task.
  M5.Mic.config(mic_cfg);
  M5.Mic.begin();
#endif  // JL_CORES3_USE_INTERNAL_MICROPHONE
#elif JL_IS_CONTROLLER(ATOM_MATRIX) || JL_IS_CONTROLLER(ATOM_S3)
  gpio_num_t clk_pin = GPIO_NUM_NC;
  gpio_num_t din_pin = GPIO_NUM_NC;
#if JL_IS_CONTROLLER(ATOM_MATRIX)
  clk_pin = GPIO_NUM_22;
  din_pin = GPIO_NUM_19;
#elif JL_IS_CONTROLLER(ATOM_S3)
  // Assuming ATOM S3U
  clk_pin = GPIO_NUM_41;
  din_pin = GPIO_NUM_42;
#endif

  i2s_pdm_rx_config_t pdm_rx_cfg = {
      .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(kSampleRate),
      .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
      .gpio_cfg =
          {
                     .clk = clk_pin,
                     .din = din_pin,
                     .invert_flags = {.clk_inv = false},
                     },
  };
  ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_handle_, &pdm_rx_cfg));
#endif

  ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));
  jll_info("I2S microphone initialized");

  // Allocate memory. For CoreS3 and Core2AWS we read stereo, so buffer must be larger.
  // 2 samples per slot * kFFTSize
  audio_buffer_ = (int16_t*)malloc(kFFTSize * 2 * sizeof(int16_t));
  fft_input_ = (float*)malloc(kFFTSize * 2 * sizeof(float));
  fft_output_ = (float*)malloc(kFFTSize * sizeof(float));
  fft_window_ = (float*)malloc(kFFTSize * sizeof(float));

  // Initialize FFT
  ESP_ERROR_CHECK(dsps_fft2r_init_fc32(nullptr, kFFTSize));
  dsps_wind_hann_f32(fft_window_, kFFTSize);
  jll_info("FFT initialized");
}

void Audio::Setup() { xTaskCreatePinnedToCore(AudioTask, "JL_Audio", 8192, this, 1, &audio_task_handle_, 1); }

void Audio::GetVisualizerData(VisualizerData* data) {
  std::lock_guard<std::mutex> lock(audio_data_mutex_);
  memcpy(data->bands, band_magnitudes_, sizeof(band_magnitudes_));
  memcpy(data->peaks, peak_magnitudes_, sizeof(peak_magnitudes_));
  data->agc_min = agc_min_;
  data->agc_max = agc_max_;
  data->volume = volume_;
  data->beat = beat_;
  data->squelch = is_squelched_;
  data->last_read_time = last_read_time_;
}

void Audio::AudioTask(void* param) {
  Audio* audio = static_cast<Audio*>(param);
  audio->Initialize();
  jll_info("Audio task started");
  while (true) {
    audio->ReadAndProcessAudio();
    vTaskDelay(1);
  }
}

void Audio::ReadAndProcessAudio() {
  size_t bytes_to_read = kFFTSize * sizeof(int16_t);
#if JL_IS_CONTROLLER(CORES3) || JL_IS_CONTROLLER(CORE2AWS)
  bytes_to_read *= 2;  // Stereo
#endif
  size_t bytes_read;
  if (i2s_channel_read(rx_handle_, audio_buffer_, bytes_to_read, &bytes_read, portMAX_DELAY) == ESP_OK) {
    bool all_zero = true;
    for (size_t i = 0; i < bytes_read / sizeof(int16_t); i++) {
      if (audio_buffer_[i] != 0) {
        all_zero = false;
        break;
      }
    }
    // Replicate M5Unified processing: noise filter and magnification
    const int32_t noise_filter_level = 16;
    const float magnification = 16.0f;
    for (int i = 0; i < kFFTSize; i++) {
      int16_t raw_val;
#if JL_IS_CONTROLLER(CORES3) || JL_IS_CONTROLLER(CORE2AWS)
      raw_val = audio_buffer_[i * 2];  // Use Left channel
#else
      raw_val = audio_buffer_[i];
#endif
      int32_t val = raw_val;
      // IIR filter: v = (val * (256 - alpha) + prev * alpha + 128) >> 8
      int32_t v = (val * (256 - noise_filter_level) + (int32_t)prev_sample_ * noise_filter_level + 128) >> 8;
      prev_sample_ = (float)v;
      float fval = (float)v * magnification;

      fft_input_[i * 2] = fval;
      fft_input_[i * 2 + 1] = 0.0f;
    }

    // Apply window and perform FFT
    dsps_mul_f32(fft_input_, fft_window_, fft_input_, kFFTSize, 2, 1, 2);
    ESP_ERROR_CHECK(dsps_fft2r_fc32(fft_input_, kFFTSize));
    dsps_bit_rev_fc32(fft_input_, kFFTSize);

    // Convert to magnitude (dB)
    for (int i = 0; i < kFFTSize / 2; i++) {
      float real = fft_input_[i * 2];
      float imag = fft_input_[i * 2 + 1];
      float power = real * real + imag * imag;
      fft_output_[i] = 10 * log10f(power + 1.0f);
    }

    // Map FFT bins to bands (logarithmic scaling)
    float min_freq = 62.5f;
    float max_freq = 8000.0f;
    float log_min = log2f(min_freq);
    float log_max = log2f(max_freq);
    float log_step = (log_max - log_min) / kNumBands;

    float new_bands[kNumBands];
    for (int i = 0; i < kNumBands; i++) {
      float start_freq = powf(2.0f, log_min + i * log_step);
      float end_freq = powf(2.0f, log_min + (i + 1) * log_step);

      int start_bin = (int)(start_freq / (kSampleRate / kFFTSize));
      int end_bin = (int)(end_freq / (kSampleRate / kFFTSize));
      if (end_bin <= start_bin) end_bin = start_bin + 1;
      if (start_bin < 1) start_bin = 1;
      if (end_bin > kFFTSize / 2) end_bin = kFFTSize / 2;

      float sum = 0;
      int count = 0;
      for (int b = start_bin; b < end_bin; b++) {
        sum += fft_output_[b];
        count++;
      }
      new_bands[i] = (count > 0) ? (sum / count) : 0;
    }

    // Squelch: If maximum band magnitude is below threshold, zero out everything
    float max_new_band_mag = 0;
    for (int i = 0; i < kNumBands; i++) {
      if (new_bands[i] > max_new_band_mag) { max_new_band_mag = new_bands[i]; }
    }

    if (max_new_band_mag < squelch_threshold_) {
      std::lock_guard<std::mutex> lock(audio_data_mutex_);
      if (!all_zero) { last_read_time_ = timeMillis(); }
      memset(band_magnitudes_, 0, sizeof(band_magnitudes_));
      memset(peak_magnitudes_, 0, sizeof(peak_magnitudes_));
      memset(prev_bands_, 0, sizeof(prev_bands_));
      volume_ = 0;
      beat_ = false;
      is_squelched_ = true;
      // We still want to update beat buffer to avoid large flux when sound returns
      beat_buffer_[beat_index_] = 0;
      beat_index_ = (beat_index_ + 1) % kBeatWindowSize;
    } else {
      // Smoothing and Peak Decay
      float smoothing = 0.4f;
      float peak_decay = 0.5f;  // dB per frame

      {
        std::lock_guard<std::mutex> lock(audio_data_mutex_);
        if (!all_zero) { last_read_time_ = timeMillis(); }
        is_squelched_ = false;
        for (int i = 0; i < kNumBands; i++) {
          band_magnitudes_[i] = band_magnitudes_[i] * smoothing + new_bands[i] * (1.0f - smoothing);
          if (new_bands[i] > peak_magnitudes_[i]) {
            peak_magnitudes_[i] = new_bands[i];
          } else {
            peak_magnitudes_[i] -= peak_decay;
            if (peak_magnitudes_[i] < 0) peak_magnitudes_[i] = 0;
          }
        }

        // AGC Tracking: Update 5-second window
        {
          float max_band_mag = 0;
          for (int i = 0; i < kNumBands; i++) {
            if (band_magnitudes_[i] > max_band_mag) max_band_mag = band_magnitudes_[i];
          }
          agc_buffer_[agc_index_] = max_band_mag;
          agc_index_ = (agc_index_ + 1) % kAgcWindowSize;

          float current_min = 100.0f;
          float current_max = -100.0f;
          bool has_data = false;
          for (int i = 0; i < kAgcWindowSize; i++) {
            if (agc_buffer_[i] > 0) {
              if (agc_buffer_[i] < current_min) current_min = agc_buffer_[i];
              if (agc_buffer_[i] > current_max) current_max = agc_buffer_[i];
              has_data = true;
            }
          }

          if (has_data) {
            if (current_max - current_min < 4.0f) {
              float center = (current_max + current_min) / 2.0f;
              current_min = center - 2.0f;
              current_max = center + 2.0f;
            }
            float agc_smoothing = 0.95f;
            agc_min_ = agc_min_ * agc_smoothing + current_min * (1.0f - agc_smoothing);
            agc_max_ = agc_max_ * agc_smoothing + current_max * (1.0f - agc_smoothing);
          }
        }

        // Calculate overall volume (average normalized magnitude)
        // Uses current agc_min/max if enabled, otherwise defaults
        float v_min = agc_enabled_ ? agc_min_ : 40.0f;
        float v_max = agc_enabled_ ? agc_max_ : 100.0f;
        float range = v_max - v_min;
        if (range < 1.0f) range = 1.0f;
        float totalNormMag = 0;
        for (int i = 0; i < kNumBands; i++) {
          float norm = (band_magnitudes_[i] - v_min) / range;
          if (norm < 0) norm = 0;
          if (norm > 1.0f) norm = 1.0f;
          totalNormMag += norm;
        }
        volume_ = totalNormMag / kNumBands;
        is_squelched_ = (volume_ < 0.4f);

        // Beat detection: Spectral Flux on first 8 bands (bass/low-mids)
        float flux = 0;
        for (int i = 0; i < 8; i++) {
          float diff = new_bands[i] - prev_bands_[i];
          if (diff > 0) flux += diff;
          prev_bands_[i] = new_bands[i];
        }

        float beat_energy = 0;
        for (int i = 0; i < 8; i++) { beat_energy += new_bands[i]; }
        beat_energy /= 8.0f;

        // Compare flux to average flux in the window
        float avg_flux = 0;
        int count = 0;
        for (int i = 0; i < kBeatWindowSize; i++) {
          if (beat_buffer_[i] > 0) {
            avg_flux += beat_buffer_[i];
            count++;
          }
        }
        avg_flux = (count > 0) ? (avg_flux / count) : 0;

        beat_ = false;
        uint32_t now = millis();
        // Trigger if flux is significantly above average OR we have a very sharp spike
        if ((flux > avg_flux * 1.3f || flux > avg_flux + 1.5f) && flux > 0.15f && beat_energy > agc_min_ - 25.0f &&
            now - last_beat_time_ > 140) {
          beat_ = true;
          last_beat_time_ = now;
        }

        beat_buffer_[beat_index_] = flux;
        beat_index_ = (beat_index_ + 1) % kBeatWindowSize;
      }
    }
  }
}

}  // namespace jazzlights

#endif  // JL_AUDIO_VISUALIZER
