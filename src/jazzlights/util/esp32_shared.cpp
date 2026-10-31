#include "jazzlights/util/esp32_shared.h"

#ifdef ESP32

#include <driver/gpio.h>
#include <esp_event.h>
#include <esp_netif.h>

#define JL_RUN_ONCE(_commands)           \
  do {                                   \
    static const bool _run_once = []() { \
      { _commands }                      \
      return true;                       \
    }();                                 \
    (void)_run_once;                     \
  } while (false)

namespace jazzlights {

void InitializeNetStack() {
  JL_RUN_ONCE({
    ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERR_INVALID_STATE means the default event loop already exists, which happens when something else (such as
    // ESPHome's network component) created it first.
    const esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_ERR_INVALID_STATE) { ESP_ERROR_CHECK(err); }
  });
}

void InstallGpioIsrService() {
  JL_RUN_ONCE({
    // ESP_ERR_INVALID_STATE means the service is already installed, which happens when something else (such as
    // ESPHome) installed it first.
    const esp_err_t err = gpio_install_isr_service(/*intrAllocFlags=*/0);
    if (err != ESP_ERR_INVALID_STATE) { ESP_ERROR_CHECK(err); }
  });
}

}  // namespace jazzlights

#endif  // ESP32
