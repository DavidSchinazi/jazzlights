#include "jazzlights/audio/audio.h"

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
  i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(kI2sPort, I2S_ROLE_MASTER);
  ESP_ERROR_CHECK(i2s_new_channel(&chanCfg, nullptr, &rxHandle_));

#if JL_IS_CONTROLLER(CORES3) || JL_IS_CONTROLLER(CORE2AWS)
  i2s_std_config_t stdCfg = {
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
  stdCfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;
#endif

  ESP_ERROR_CHECK(i2s_channel_init_std_mode(rxHandle_, &stdCfg));
#if JL_IS_CONTROLLER(CORES3) && JL_CORES3_USE_INTERNAL_MICROPHONE
  auto micCfg = M5.Mic.config();
  micCfg.pin_data_in = -1;  // Prevent M5Unified from initializing I2S and starting a task.
  M5.Mic.config(micCfg);
  M5.Mic.begin();
#endif  // JL_CORES3_USE_INTERNAL_MICROPHONE
#elif JL_IS_CONTROLLER(ATOM_MATRIX) || JL_IS_CONTROLLER(ATOM_S3) || JL_IS_CONTROLLER(M5STICK_C)
  gpio_num_t clkPin = GPIO_NUM_NC;
  gpio_num_t dinPin = GPIO_NUM_NC;
#if JL_IS_CONTROLLER(ATOM_MATRIX)
  clkPin = GPIO_NUM_22;
  dinPin = GPIO_NUM_19;
#elif JL_IS_CONTROLLER(ATOM_S3)
  // Assuming ATOM S3U
  clkPin = GPIO_NUM_41;
  dinPin = GPIO_NUM_42;
#elif JL_IS_CONTROLLER(M5STICK_C)
  // This doesn't actually work yet.
  clkPin = GPIO_NUM_0;
  dinPin = GPIO_NUM_34;
#endif

  i2s_pdm_rx_config_t pdmRxCfg = {
      .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(kSampleRate),
      .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
      .gpio_cfg =
          {
                     .clk = clkPin,
                     .din = dinPin,
                     .invert_flags = {.clk_inv = false},
                     },
  };
  ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rxHandle_, &pdmRxCfg));
#endif

  ESP_ERROR_CHECK(i2s_channel_enable(rxHandle_));
  jll_info("I2S microphone initialized");

  // Allocate memory. For CoreS3 and Core2AWS we read stereo, so buffer must be larger.
  // 2 samples per slot * kFFTSize
  audioBuffer_ = (int16_t*)malloc(kFFTSize * 2 * sizeof(int16_t));
  fftInput_ = (float*)malloc(kFFTSize * 2 * sizeof(float));
  fftOutput_ = (float*)malloc(kFFTSize * sizeof(float));
  fftWindow_ = (float*)malloc(kFFTSize * sizeof(float));

  // Initialize FFT
  ESP_ERROR_CHECK(dsps_fft2r_init_fc32(nullptr, kFFTSize));
  dsps_wind_hann_f32(fftWindow_, kFFTSize);
  jll_info("FFT initialized");
}

void Audio::Setup() { xTaskCreatePinnedToCore(AudioTask, "JL_Audio", 8192, this, 1, &audioTaskHandle_, 1); }

void Audio::GetVisualizerData(VisualizerData* data) {
  std::lock_guard<std::mutex> lock(audioDataMutex_);
  memcpy(data->bands, bandMagnitudes_, sizeof(bandMagnitudes_));
  memcpy(data->peaks, peakMagnitudes_, sizeof(peakMagnitudes_));
  data->agcMin = agcMin_;
  data->agcMax = agcMax_;
  data->volume = volume_;
  data->beat = beat_;
  data->squelch = isSquelched_;
  data->lastReadTime = lastReadTime_;
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
  size_t bytesToRead = kFFTSize * sizeof(int16_t);
#if JL_IS_CONTROLLER(CORES3) || JL_IS_CONTROLLER(CORE2AWS)
  bytesToRead *= 2;  // Stereo
#endif
  size_t bytesRead;
  if (i2s_channel_read(rxHandle_, audioBuffer_, bytesToRead, &bytesRead, portMAX_DELAY) == ESP_OK) {
    bool allZero = true;
    for (size_t i = 0; i < bytesRead / sizeof(int16_t); i++) {
      if (audioBuffer_[i] != 0) {
        allZero = false;
        break;
      }
    }
    // Replicate M5Unified processing: noise filter and magnification
    const int32_t noiseFilterLevel = 16;
    const float magnification = 16.0f;
    for (int i = 0; i < kFFTSize; i++) {
      int16_t rawVal;
#if JL_IS_CONTROLLER(CORES3) || JL_IS_CONTROLLER(CORE2AWS)
      rawVal = audioBuffer_[i * 2];  // Use Left channel
#else
      rawVal = audioBuffer_[i];
#endif
      int32_t val = rawVal;
      // IIR filter: v = (val * (256 - alpha) + prev * alpha + 128) >> 8
      int32_t v = (val * (256 - noiseFilterLevel) + (int32_t)prevSample_ * noiseFilterLevel + 128) >> 8;
      prevSample_ = (float)v;
      float fval = (float)v * magnification;

      fftInput_[i * 2] = fval;
      fftInput_[i * 2 + 1] = 0.0f;
    }

    // Apply window and perform FFT
    dsps_mul_f32(fftInput_, fftWindow_, fftInput_, kFFTSize, 2, 1, 2);
    ESP_ERROR_CHECK(dsps_fft2r_fc32(fftInput_, kFFTSize));
    dsps_bit_rev_fc32(fftInput_, kFFTSize);

    // Convert to magnitude (dB)
    for (int i = 0; i < kFFTSize / 2; i++) {
      float real = fftInput_[i * 2];
      float imag = fftInput_[i * 2 + 1];
      float power = real * real + imag * imag;
      fftOutput_[i] = 10 * log10f(power + 1.0f);
    }

    // Map FFT bins to bands (logarithmic scaling)
    float minFreq = 62.5f;
    float maxFreq = 8000.0f;
    float logMin = log2f(minFreq);
    float logMax = log2f(maxFreq);
    float logStep = (logMax - logMin) / kNumBands;

    float newBands[kNumBands];
    for (int i = 0; i < kNumBands; i++) {
      float startFreq = powf(2.0f, logMin + i * logStep);
      float endFreq = powf(2.0f, logMin + (i + 1) * logStep);

      int startBin = (int)(startFreq / (kSampleRate / kFFTSize));
      int endBin = (int)(endFreq / (kSampleRate / kFFTSize));
      if (endBin <= startBin) endBin = startBin + 1;
      if (startBin < 1) startBin = 1;
      if (endBin > kFFTSize / 2) endBin = kFFTSize / 2;

      float sum = 0;
      int count = 0;
      for (int b = startBin; b < endBin; b++) {
        sum += fftOutput_[b];
        count++;
      }
      newBands[i] = (count > 0) ? (sum / count) : 0;
    }

    // Squelch: If maximum band magnitude is below threshold, zero out everything
    float maxNewBandMag = 0;
    for (int i = 0; i < kNumBands; i++) {
      if (newBands[i] > maxNewBandMag) { maxNewBandMag = newBands[i]; }
    }

    if (maxNewBandMag < squelchThreshold_) {
      std::lock_guard<std::mutex> lock(audioDataMutex_);
      if (!allZero) { lastReadTime_ = TimeMicros(); }
      memset(bandMagnitudes_, 0, sizeof(bandMagnitudes_));
      memset(peakMagnitudes_, 0, sizeof(peakMagnitudes_));
      memset(prevBands_, 0, sizeof(prevBands_));
      volume_ = 0;
      beat_ = false;
      isSquelched_ = true;
      // We still want to update beat buffer to avoid large flux when sound returns
      beatBuffer_[beatIndex_] = 0;
      beatIndex_ = (beatIndex_ + 1) % kBeatWindowSize;
    } else {
      // Smoothing and Peak Decay
      float smoothing = 0.4f;
      float peakDecay = 0.5f;  // dB per frame

      {
        std::lock_guard<std::mutex> lock(audioDataMutex_);
        if (!allZero) { lastReadTime_ = TimeMicros(); }
        isSquelched_ = false;
        for (int i = 0; i < kNumBands; i++) {
          bandMagnitudes_[i] = bandMagnitudes_[i] * smoothing + newBands[i] * (1.0f - smoothing);
          if (newBands[i] > peakMagnitudes_[i]) {
            peakMagnitudes_[i] = newBands[i];
          } else {
            peakMagnitudes_[i] -= peakDecay;
            if (peakMagnitudes_[i] < 0) peakMagnitudes_[i] = 0;
          }
        }

        // AGC Tracking: Update 5-second window
        {
          float maxBandMag = 0;
          for (int i = 0; i < kNumBands; i++) {
            if (bandMagnitudes_[i] > maxBandMag) maxBandMag = bandMagnitudes_[i];
          }
          agcBuffer_[agcIndex_] = maxBandMag;
          agcIndex_ = (agcIndex_ + 1) % kAgcWindowSize;

          float currentMin = 100.0f;
          float currentMax = -100.0f;
          bool hasData = false;
          for (int i = 0; i < kAgcWindowSize; i++) {
            if (agcBuffer_[i] > 0) {
              if (agcBuffer_[i] < currentMin) currentMin = agcBuffer_[i];
              if (agcBuffer_[i] > currentMax) currentMax = agcBuffer_[i];
              hasData = true;
            }
          }

          if (hasData) {
            if (currentMax - currentMin < 4.0f) {
              float center = (currentMax + currentMin) / 2.0f;
              currentMin = center - 2.0f;
              currentMax = center + 2.0f;
            }
            float agcSmoothing = 0.95f;
            agcMin_ = agcMin_ * agcSmoothing + currentMin * (1.0f - agcSmoothing);
            agcMax_ = agcMax_ * agcSmoothing + currentMax * (1.0f - agcSmoothing);
          }
        }

        // Calculate overall volume (average normalized magnitude)
        // Uses current agcMin/max if enabled, otherwise defaults
        float vMin = agcEnabled_ ? agcMin_ : 40.0f;
        float vMax = agcEnabled_ ? agcMax_ : 100.0f;
        float range = vMax - vMin;
        if (range < 1.0f) range = 1.0f;
        float totalNormMag = 0;
        for (int i = 0; i < kNumBands; i++) {
          float norm = (bandMagnitudes_[i] - vMin) / range;
          if (norm < 0) norm = 0;
          if (norm > 1.0f) norm = 1.0f;
          totalNormMag += norm;
        }
        volume_ = totalNormMag / kNumBands;
        isSquelched_ = (volume_ < 0.4f);

        // Beat detection: Spectral Flux on first 8 bands (bass/low-mids)
        float flux = 0;
        for (int i = 0; i < 8; i++) {
          float diff = newBands[i] - prevBands_[i];
          if (diff > 0) flux += diff;
          prevBands_[i] = newBands[i];
        }

        float beatEnergy = 0;
        for (int i = 0; i < 8; i++) { beatEnergy += newBands[i]; }
        beatEnergy /= 8.0f;

        // Compare flux to average flux in the window
        float avgFlux = 0;
        int count = 0;
        for (int i = 0; i < kBeatWindowSize; i++) {
          if (beatBuffer_[i] > 0) {
            avgFlux += beatBuffer_[i];
            count++;
          }
        }
        avgFlux = (count > 0) ? (avgFlux / count) : 0;

        beat_ = false;
        Microseconds now = TimeMicros();
        // Trigger if flux is significantly above average OR we have a very sharp spike
        if ((flux > avgFlux * 1.3f || flux > avgFlux + 1.5f) && flux > 0.15f && beatEnergy > agcMin_ - 25.0f &&
            now - lastBeatTime_ > 140 * kMicrosecondsPerMillisecond) {
          beat_ = true;
          lastBeatTime_ = now;
        }

        beatBuffer_[beatIndex_] = flux;
        beatIndex_ = (beatIndex_ + 1) % kBeatWindowSize;
      }
    }
  }
}

}  // namespace jazzlights

#endif  // JL_AUDIO_VISUALIZER
