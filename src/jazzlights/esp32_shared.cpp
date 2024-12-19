#include "jazzlights/esp32_shared.h"

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
    ESP_ERROR_CHECK(esp_event_loop_create_default());
  });
}

void InstallGpioIsrService() {
  JL_RUN_ONCE({ ESP_ERROR_CHECK(gpio_install_isr_service(/*intr_alloc_flags=*/0)); });
}

}  // namespace jazzlights

#endif  // ESP32
