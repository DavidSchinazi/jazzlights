#ifndef JL_SHIM_ESP_APP_DESC_H
#define JL_SHIM_ESP_APP_DESC_H

struct esp_app_desc_t {
  char project_name[32];
};

const esp_app_desc_t* esp_app_get_description();

#endif  // JL_SHIM_ESP_APP_DESC_H
