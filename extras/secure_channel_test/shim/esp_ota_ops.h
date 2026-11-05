#ifndef JL_SHIM_ESP_OTA_OPS_H
#define JL_SHIM_ESP_OTA_OPS_H

#include <cstddef>
#include <cstdint>

#include "esp_app_desc.h"

// A fake of the parts of ESP-IDF's app_update that OTAHandler uses, which keeps the partition in memory. See main.cpp.

using esp_err_t = int;
inline constexpr esp_err_t ESP_OK = 0;
inline constexpr esp_err_t ESP_FAIL = -1;
inline constexpr size_t OTA_WITH_SEQUENTIAL_WRITES = 0xfffffffe;
const char* esp_err_to_name(esp_err_t err);

using esp_ota_handle_t = uint32_t;

struct esp_partition_t {
  uint32_t size;
};

enum esp_ota_img_states_t {
  ESP_OTA_IMG_VALID,
  ESP_OTA_IMG_PENDING_VERIFY,
};

const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t* start);
const esp_partition_t* esp_ota_get_running_partition();
esp_err_t esp_ota_get_state_partition(const esp_partition_t* partition, esp_ota_img_states_t* state);
esp_err_t esp_ota_begin(const esp_partition_t* partition, size_t imageSize, esp_ota_handle_t* handle);
esp_err_t esp_ota_write(esp_ota_handle_t handle, const void* data, size_t size);
esp_err_t esp_ota_end(esp_ota_handle_t handle);
esp_err_t esp_ota_abort(esp_ota_handle_t handle);
esp_err_t esp_ota_get_partition_description(const esp_partition_t* partition, esp_app_desc_t* description);
esp_err_t esp_ota_set_boot_partition(const esp_partition_t* partition);
esp_err_t esp_ota_mark_app_valid_cancel_rollback();
esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot();

#endif  // JL_SHIM_ESP_OTA_OPS_H
