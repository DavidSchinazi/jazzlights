#include "jazzlights/audio.h"

#if JL_AUDIO_VISUALIZER

#include <Arduino.h>
#include <M5Unified.h>
#include <esp_dsp.h>
#include <esp_log.h>

#include <cmath>
#include <cstring>

#include "jazzlights/util/log.h"

namespace jazzlights {

// FFT constants
#define FFT_N 256
#define SAMPLE_RATE 16000

Audio& Audio::Get() {
  static Audio instance;
  return instance;
}

void Audio::Initialize() {
  if (audio_buffer_ != nullptr) {
    return;  // Already initialized
  }
  jll_info("Starting audio setup...");

  // Setup microphone using M5Unified
  auto mic_cfg = M5.Mic.config();
  mic_cfg.sample_rate = SAMPLE_RATE;
  mic_cfg.stereo = false;
  mic_cfg.magnification = 8;
  mic_cfg.noise_filter_level = 4;
  M5.Mic.config(mic_cfg);
  M5.Mic.begin();
  jll_info("M5 microphone initialized");

  // Allocate memory
  audio_buffer_ = (int16_t*)malloc(FFT_N * sizeof(int16_t));
  fft_input_ = (float*)malloc(FFT_N * 2 * sizeof(float));
  fft_output_ = (float*)malloc(FFT_N * sizeof(float));
  fft_window_ = (float*)malloc(FFT_N * sizeof(float));

  // Initialize FFT
  ESP_ERROR_CHECK(dsps_fft2r_init_fc32(NULL, FFT_N));
  dsps_wind_hann_f32(fft_window_, FFT_N);
  jll_info("FFT initialized");

  // Start audio task
  xTaskCreatePinnedToCore(AudioTask, "AudioTask", 8192, this, 1, &audio_task_handle_, 1);
  jll_info("Audio task created");

  jll_info("Audio setup complete");
}

void Audio::GetVisualizerData(VisualizerData* data) {
  std::lock_guard<std::mutex> lock(audio_data_mutex_);
  memcpy(data->bands, band_magnitudes_, sizeof(band_magnitudes_));
  memcpy(data->peaks, peak_magnitudes_, sizeof(peak_magnitudes_));
  data->agc_min = agc_min_;
  data->agc_max = agc_max_;
  data->beat = beat_;
}

void Audio::AudioTask(void* param) {
  Audio* audio = static_cast<Audio*>(param);
  jll_info("Audio task started");
  uint32_t last_beat_time = 0;
  while (true) {
    if (M5.Mic.record(audio->audio_buffer_, FFT_N, SAMPLE_RATE)) {
      for (int i = 0; i < FFT_N; i++) {
        audio->fft_input_[i * 2] = (float)audio->audio_buffer_[i];
        audio->fft_input_[i * 2 + 1] = 0.0f;
      }

      // Apply window and perform FFT
      dsps_mul_f32(audio->fft_input_, audio->fft_window_, audio->fft_input_, FFT_N, 2, 1, 2);
      ESP_ERROR_CHECK(dsps_fft2r_fc32(audio->fft_input_, FFT_N));
      dsps_bit_rev_fc32(audio->fft_input_, FFT_N);

      // Convert to magnitude (dB)
      for (int i = 0; i < FFT_N / 2; i++) {
        float real = audio->fft_input_[i * 2];
        float imag = audio->fft_input_[i * 2 + 1];
        float power = real * real + imag * imag;
        audio->fft_output_[i] = 10 * log10f(power + 1.0f);
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

        int start_bin = (int)(start_freq / (SAMPLE_RATE / FFT_N));
        int end_bin = (int)(end_freq / (SAMPLE_RATE / FFT_N));
        if (end_bin <= start_bin) end_bin = start_bin + 1;
        if (start_bin < 1) start_bin = 1;
        if (end_bin > FFT_N / 2) end_bin = FFT_N / 2;

        float sum = 0;
        int count = 0;
        for (int b = start_bin; b < end_bin; b++) {
          sum += audio->fft_output_[b];
          count++;
        }
        new_bands[i] = (count > 0) ? (sum / count) : 0;
      }

      // Smoothing and Peak Decay
      float smoothing = 0.7f;
      float peak_decay = 0.5f;  // dB per frame

      {
        std::lock_guard<std::mutex> lock(audio->audio_data_mutex_);
        for (int i = 0; i < kNumBands; i++) {
          audio->band_magnitudes_[i] = audio->band_magnitudes_[i] * smoothing + new_bands[i] * (1.0f - smoothing);
          if (new_bands[i] > audio->peak_magnitudes_[i]) {
            audio->peak_magnitudes_[i] = new_bands[i];
          } else {
            audio->peak_magnitudes_[i] -= peak_decay;
            if (audio->peak_magnitudes_[i] < 0) audio->peak_magnitudes_[i] = 0;
          }
        }

        // AGC Tracking: Update 5-second window
        float max_band_mag = 0;
        for (int i = 0; i < kNumBands; i++) {
          if (audio->band_magnitudes_[i] > max_band_mag) max_band_mag = audio->band_magnitudes_[i];
        }
        audio->agc_buffer_[audio->agc_index_] = max_band_mag;
        audio->agc_index_ = (audio->agc_index_ + 1) % kAgcWindowSize;

        float current_min = 100.0f;
        float current_max = -100.0f;
        bool has_data = false;
        for (int i = 0; i < kAgcWindowSize; i++) {
          if (audio->agc_buffer_[i] > 0) {
            if (audio->agc_buffer_[i] < current_min) current_min = audio->agc_buffer_[i];
            if (audio->agc_buffer_[i] > current_max) current_max = audio->agc_buffer_[i];
            has_data = true;
          }
        }

        if (has_data) {
          if (current_max - current_min < 10.0f) {
            float center = (current_max + current_min) / 2.0f;
            current_min = center - 5.0f;
            current_max = center + 5.0f;
          }
          float agc_smoothing = 0.95f;
          audio->agc_min_ = audio->agc_min_ * agc_smoothing + current_min * (1.0f - agc_smoothing);
          audio->agc_max_ = audio->agc_max_ * agc_smoothing + current_max * (1.0f - agc_smoothing);
        }

        // Beat detection: Spectral Flux on first 8 bands (bass/low-mids)
        float flux = 0;
        static float prev_bands[8] = {0};
        for (int i = 0; i < 8; i++) {
          float diff = new_bands[i] - prev_bands[i];
          if (diff > 0) flux += diff;
          prev_bands[i] = new_bands[i];
        }

        float beat_energy = 0;
        for (int i = 0; i < 8; i++) { beat_energy += new_bands[i]; }
        beat_energy /= 8.0f;

        // Compare flux to average flux in the window
        float avg_flux = 0;
        int count = 0;
        for (int i = 0; i < kBeatWindowSize; i++) {
          if (audio->beat_buffer_[i] > 0) {
            avg_flux += audio->beat_buffer_[i];
            count++;
          }
        }
        avg_flux = (count > 0) ? (avg_flux / count) : 0;

        audio->beat_ = false;
        uint32_t now = millis();
        // Trigger if flux is significantly above average OR we have a very sharp spike
        if ((flux > avg_flux * 1.3f || flux > avg_flux + 1.5f) && flux > 0.15f &&
            beat_energy > audio->agc_min_ - 25.0f && now - last_beat_time > 140) {
          audio->beat_ = true;
          last_beat_time = now;
        }

        audio->beat_buffer_[audio->beat_index_] = flux;
        audio->beat_index_ = (audio->beat_index_ + 1) % kBeatWindowSize;
      }
    }
    vTaskDelay(1);
  }
}

}  // namespace jazzlights

#endif  // JL_AUDIO_VISUALIZER
