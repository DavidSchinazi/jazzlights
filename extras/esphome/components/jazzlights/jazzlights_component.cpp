#include "esphome/components/jazzlights/jazzlights_component.h"

#include <esp_wifi.h>

#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/log.h"
#include "jazzlights/network/esp32_socket_manager.h"
#include "jazzlights/util/instrumentation.h"
#include "jazzlights/util/log.h"

namespace esphome::jazzlights_esphome {

static const char* const TAG = "jazzlights";

namespace {

// Forwards JazzLights logs to ESPHome's logger, so they also show up over Wi-Fi. The tag is the source file name.
void LogToEsphome(char level, const char* file, int line, const char* format, va_list args) {
  int esphomeLevel;
  switch (level) {
    case 'D': esphomeLevel = ESPHOME_LOG_LEVEL_DEBUG; break;
    case 'E':
    case 'F': esphomeLevel = ESPHOME_LOG_LEVEL_ERROR; break;
    default: esphomeLevel = ESPHOME_LOG_LEVEL_INFO; break;
  }
  ::esphome::esp_log_vprintf_(esphomeLevel, file, line, format, args);
}

}  // namespace

void JazzLightsComponent::setup() {
  // This needs to happen before SetupPrimaryRunLoop() starts our other tasks.
  ::jazzlights::SetLogHandler(&LogToEsphome);
  ::jazzlights::SetupPrimaryRunLoop();
  // JazzLights renders as fast as it can, so we don't want ESPHome to sleep between loop iterations.
  this->high_frequency_loop_requester_.start();
}

void JazzLightsComponent::loop() {
  {
    JL_LOG_IF_SLOW("Adding Wi-Fi interface");
    this->maybe_add_wifi_interface_();
  }
  ::jazzlights::RunPrimaryRunLoop();
}

void JazzLightsComponent::maybe_add_wifi_interface_() {
  if (this->wifi_interface_added_) { return; }
  // ESPHome creates its Wi-Fi interface during the Wi-Fi component's setup, which runs after ours.
  esp_netif_t* netif = wifi::global_wifi_component->get_esp_netif_sta();
  if (netif == nullptr) { return; }
  uint8_t macAddress[6];
  if (esp_wifi_get_mac(WIFI_IF_STA, macAddress) != ESP_OK) { return; }
  ::jazzlights::Esp32SocketManager::Get()->AddInterface(netif, ::jazzlights::NetworkType::kWiFi,
                                                        ::jazzlights::NetworkDeviceId(macAddress));
  this->wifi_interface_added_ = true;
}

void JazzLightsComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "JazzLights:\n"
                "  Boot message: %s\n"
                "  Wi-Fi: %s",
                BOOT_MESSAGE, ::jazzlights::Esp32SocketManager::Get()->GetStatusStr().c_str());
}

}  // namespace esphome::jazzlights_esphome
