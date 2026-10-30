#ifndef JL_FASTLED_IDF_SHIM_H
#define JL_FASTLED_IDF_SHIM_H

// FastLED expects the Arduino core on ESP32, but only uses a handful of its functions and macros. This header provides
// them on top of ESP-IDF so that we can build FastLED with `framework = espidf`. It is only used by environments that
// do not use the Arduino core, see extras/scripts/fastled_idf_shim.py. The definitions match the Arduino core's.
// This is only tested with the FastLED version pinned in platformio.ini, upgrading FastLED might require more.
// This header needs to remain valid C since FastLED contains some C files.

#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_rom_sys.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <math.h>
#include <soc/gpio_reg.h>
#include <soc/soc_caps.h>
#include <stdbool.h>
#include <stdint.h>

#if CONFIG_SPIRAM
#include <esp_psram.h>
#endif  // CONFIG_SPIRAM

#define LOW 0x0
#define HIGH 0x1

#define INPUT 0x01
#define OUTPUT 0x03
#define INPUT_PULLUP 0x05

#define digitalPinToPort(pin) (((pin) > 31) ? 1 : 0)
#define digitalPinToBitMask(pin) (1UL << (((pin) > 31) ? ((pin) - 32) : (pin)))
#ifdef GPIO_OUT1_REG
#define portOutputRegister(port) ((volatile uint32_t*)((port) ? GPIO_OUT1_REG : GPIO_OUT_REG))
#define portInputRegister(port) ((volatile uint32_t*)((port) ? GPIO_IN1_REG : GPIO_IN_REG))
#else  // GPIO_OUT1_REG
#define portOutputRegister(port) ((volatile uint32_t*)GPIO_OUT_REG)
#define portInputRegister(port) ((volatile uint32_t*)GPIO_IN_REG)
#endif  // GPIO_OUT1_REG

static inline unsigned long millis(void) { return (unsigned long)(esp_timer_get_time() / 1000); }

static inline unsigned long micros(void) { return (unsigned long)esp_timer_get_time(); }

static inline void delay(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

static inline void delayMicroseconds(uint32_t us) { esp_rom_delay_us(us); }

static inline void yield(void) { taskYIELD(); }

static inline void pinMode(uint8_t pin, uint8_t mode) {
  gpio_config_t config = {};
  config.pin_bit_mask = 1ULL << pin;
  config.mode = (mode == OUTPUT) ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT;
  config.pull_up_en = (mode == INPUT_PULLUP) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
  config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  config.intr_type = GPIO_INTR_DISABLE;
  (void)gpio_config(&config);
}

// The ESP32-S3 I2S driver uses the following. The Arduino core enables CONFIG_FREERTOS_ENABLE_BACKWARD_COMPATIBILITY,
// which provides the legacy FreeRTOS type name.
#ifndef xSemaphoreHandle
#define xSemaphoreHandle SemaphoreHandle_t
#endif  // xSemaphoreHandle

#ifndef log_e
#define log_e(format, ...) ESP_LOGE("FastLED", format, ##__VA_ARGS__)
#endif  // log_e

static inline bool psramInit(void) {
#if CONFIG_SPIRAM
  return esp_psram_is_initialized();
#else   // CONFIG_SPIRAM
  return false;
#endif  // CONFIG_SPIRAM
}

static inline int digitalRead(uint8_t pin) { return gpio_get_level((gpio_num_t)pin); }

static inline void digitalWrite(uint8_t pin, uint8_t val) { (void)gpio_set_level((gpio_num_t)pin, val); }

#endif  // JL_FASTLED_IDF_SHIM_H
