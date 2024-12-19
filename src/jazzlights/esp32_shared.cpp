#include "jazzlights/esp32_shared.h"

#ifdef ESP32

#include <esp_event.h>
#include <esp_netif.h>

namespace jazzlights {

void InitializeNetStack() {
  static const bool run_once = []() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    return true;
  }();
  (void)run_once;
}

}  // namespace jazzlights

#endif  // ESP32
