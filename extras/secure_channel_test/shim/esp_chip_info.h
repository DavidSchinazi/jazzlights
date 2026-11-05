#ifndef JL_SHIM_ESP_CHIP_INFO_H
#define JL_SHIM_ESP_CHIP_INFO_H

// Same value as ESP32 in the real enum, where it isn't 0 as it is in the image header.
enum esp_chip_model_t {
  CHIP_ESP32 = 1,
};

struct esp_chip_info_t {
  esp_chip_model_t model;
};

inline void esp_chip_info(esp_chip_info_t* info) { info->model = CHIP_ESP32; }

#endif  // JL_SHIM_ESP_CHIP_INFO_H
